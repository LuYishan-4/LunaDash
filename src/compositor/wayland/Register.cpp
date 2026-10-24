#include "compositor/wayland/Register.hpp"
#include "compositor/renderer/selection/RenderSelection.h"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/log/Log.hpp"
#include <algorithm>
#include <cerrno>
#include <glib.h>
#include <poll.h>
#include <vector>

namespace LunaDash {
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

WaylandCompositor::Impl::Impl(WaylandCompositor *owner,
                              const std::string &socket, bool wantsFullscreen,
                              const std::string &renderer)
    : q(owner), socketName(socket), fullscreen(wantsFullscreen),
      nested(!environmentValue("WAYLAND_DISPLAY").empty() ||
             !environmentValue("DISPLAY").empty()),
      rendererPreference(renderer) {
  display = wl_display_create();
  if (display)
    eventLoop = wl_display_get_event_loop(display);
}

WaylandCompositor::Impl::~Impl() { shutdown(); }

bool WaylandCompositor::Impl::initialize() {
  wlr_log_init(WLR_INFO, nullptr);
  if (!display || !eventLoop)
    return fail("Could not create the Wayland display.");

  backend = WlrootsCompat::createPlatform(display);
  if (!backend)
    return fail("wlroots could not create a backend.");
  renderer = ludash_renderer_create(backend, rendererPreference.c_str());
  if (!renderer)
    return fail("wlroots could not create a renderer.");
#if WLR_VERSION_MINOR >= 20
  // Keep ownership of linux-dmabuf so the scene graph can publish
  // per-surface feedback. Modern Chromium/Electron clients use that feedback
  // to choose importable modifiers instead of guessing from a global list.
  if (!wlr_renderer_init_wl_shm(renderer, display))
    return fail("wlroots could not initialize shared-memory formats.");
#else
  if (!wlr_renderer_init_wl_display(renderer, display))
    return fail("wlroots could not initialize renderer formats.");
#endif
  allocator = wlr_allocator_autocreate(backend, renderer);
  if (!allocator)
    return fail("wlroots could not create a buffer allocator.");

  compositor = wlr_compositor_create(display, 5, renderer);
  if (!compositor)
    return fail("wlroots could not create the compositor global.");
  wlr_subcompositor_create(display);
  wlr_data_device_manager_create(display);
#if LUDASH_WLR_HAS_DATA_CONTROL
  // Clipboard managers such as wl-paste --watch need the privileged
  // wlr-data-control protocol; wl_data_device alone is focus-bound and cannot
  // provide global clipboard history.
  dataControl = wlr_data_control_manager_v1_create(display);
  if (!dataControl)
    return fail("wlroots could not create the data-control manager.");
#endif
  wlr_viewporter_create(display);
#if WLR_VERSION_MINOR >= 20
  if (!wlr_single_pixel_buffer_manager_v1_create(display))
    return fail("wlroots could not create the single-pixel buffer global.");
#endif

  outputLayout = WlrootsCompat::createOutputLayout(display);
  if (!outputLayout)
    return fail("wlroots could not create the output layout.");

  scene = wlr_scene_create();
  if (!scene)
    return fail("wlroots could not create the scene graph.");
#if WLR_VERSION_MINOR >= 20
  if (wlr_renderer_get_drm_fd(renderer) >= 0 &&
      wlr_renderer_get_texture_formats(renderer, WLR_BUFFER_CAP_DMABUF)) {
    if (!wlr_drm_create(display, renderer))
      return fail("wlroots could not create the DRM buffer global.");
    auto *linuxDmabuf =
        wlr_linux_dmabuf_v1_create_with_renderer(display, 4, renderer);
    if (!linuxDmabuf)
      return fail("wlroots could not create linux-dmabuf feedback.");
    wlr_scene_set_linux_dmabuf_v1(scene, linuxDmabuf);
  }
#endif
#if LUDASH_WLR_HAS_DRM_SYNCOBJ && WLR_VERSION_MINOR >= 19
  // wlroots 0.18 ships the protocol header, but renderer/backend timeline
  // feature flags were added in 0.19.
  // Modern Chromium/Electron clients can use explicit DRM timeline fences.
  // wlroots' scene-surface helper integrates this protocol with scene
  // rendering, but it is safe to advertise only when both ends support
  // timeline synchronization.
  const int rendererDrmFd = wlr_renderer_get_drm_fd(renderer);
  if (rendererDrmFd >= 0 && renderer->features.timeline &&
      backend->features.timeline) {
    explicitSync =
        wlr_linux_drm_syncobj_manager_v1_create(display, 1, rendererDrmFd);
    if (!explicitSync)
      logWarning("LunaDash: DRM explicit synchronization is unavailable.");
  }
#endif
  sceneLayout = wlr_scene_attach_output_layout(scene, outputLayout);
  backgroundLayer = wlr_scene_tree_create(&scene->tree);
  bottomLayer = wlr_scene_tree_create(&scene->tree);
  normalLayer = wlr_scene_tree_create(&scene->tree);
  animationLayer = wlr_scene_tree_create(&scene->tree);
  topLayer = wlr_scene_tree_create(&scene->tree);
  // True xdg fullscreen sits above ordinary top-layer panels. Layer-shell
  // OVERLAY stays above it for OSD/recorder overlays and selectors.
  fullscreenLayer = wlr_scene_tree_create(&scene->tree);
  overlayLayer = wlr_scene_tree_create(&scene->tree);
  const float color[4] = {0.043f, 0.067f, 0.078f, 1.0f};
  background = wlr_scene_rect_create(backgroundLayer, 1440, 900, color);
  windowGlass = std::make_unique<WindowGlass>(renderer, allocator,
                                             &scene->tree.node);

  windowCorners = std::make_unique<WindowCorners>();

  xdgShell = wlr_xdg_shell_create(display, 3);
  layerShell = wlr_layer_shell_v1_create(display, 4);
#if LUDASH_WLR_HAS_XDG_ACTIVATION
  // xdg-activation is optional in older wlroots builds. Its requests are
  // filtered through the seat's event-serial history before focus changes.
  xdgActivation = wlr_xdg_activation_v1_create(display);
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  // Keep the wlroots toplevel-management compatibility global alongside the
  // newer ext-foreign-toplevel-list protocol. Native Wayland applications
  // such as GPU Screen Recorder UI still probe this interface for the active
  // toplevel and its title/app-id.
  foreignToplevelManager = wlr_foreign_toplevel_manager_v1_create(display);
#endif
  screencopy = wlr_screencopy_manager_v1_create(display);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  foreignToplevelList = wlr_ext_foreign_toplevel_list_v1_create(display, 1);
  imageCopyCapture = wlr_ext_image_copy_capture_manager_v1_create(display, 1);
  // xdg-desktop-portal-wlr uses this manager for the modern monitor
  // ScreenCast path. Without it, xdpw must fall back to screencopy+dmabuf,
  // which is not available on every renderer/GPU combination.
  if (!wlr_ext_output_image_capture_source_manager_v1_create(display, 1))
    return fail("wlroots could not create output image capture sources.");
  foreignToplevelCaptureSource =
      wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(display, 1);
#endif
  xdgOutput = wlr_xdg_output_manager_v1_create(display, outputLayout);
  idleInhibit = wlr_idle_inhibit_v1_create(display);
  inputMethodManager = wlr_input_method_manager_v2_create(display);
  textInputManager = wlr_text_input_manager_v3_create(display);
  virtualKeyboardManager = wlr_virtual_keyboard_manager_v1_create(display);
  if (!xdgShell || !layerShell || !screencopy || !xdgOutput || !idleInhibit ||
      !inputMethodManager || !textInputManager || !virtualKeyboardManager)
    return fail("wlroots could not create required Wayland protocol globals.");
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  if (!foreignToplevelManager)
    return fail("wlroots could not create foreign-toplevel management.");
#endif
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  if (!foreignToplevelList || !imageCopyCapture || !foreignToplevelCaptureSource)
    return fail("wlroots could not create window capture protocol globals.");
#endif

  seat = wlr_seat_create(display, "seat0");
  cursor = wlr_cursor_create();
  cursorManager = wlr_xcursor_manager_create(
      nullptr, environmentInt("XCURSOR_SIZE") > 0
                   ? static_cast<uint32_t>(environmentInt("XCURSOR_SIZE"))
                   : 24);
  if (!seat || !cursor || !cursorManager)
    return fail("wlroots could not create the seat/cursor stack.");
  wlr_cursor_attach_output_layout(cursor, outputLayout);

  attachListener(&backend->events.new_output, newOutput, this, handleNewOutput);
  attachListener(&backend->events.new_input, newInput, this, handleNewInput);
#if WLR_VERSION_MINOR < 18
  attachListener(&xdgShell->events.new_surface, newXdgSurface, this,
                 handleNewXdgSurface);
#else
  // Since wlroots 0.18, xdg_shell exposes role-specific new_toplevel events.
  // Track the role as soon as it is created, then wait for initial_commit.
  attachListener(&xdgShell->events.new_toplevel, newXdgSurface, this,
                 handleNewXdgToplevel);
  attachListener(&xdgShell->events.new_popup, newXdgPopup, this,
                 handleNewXdgPopup);
#endif
  attachListener(&layerShell->events.new_surface, newLayerSurface, this,
                 handleNewLayerSurface);
#if LUDASH_WLR_HAS_XDG_ACTIVATION
  if (xdgActivation)
    attachListener(&xdgActivation->events.request_activate,
                   requestXdgActivation, this, handleXdgActivationRequest);
#endif
  attachListener(&seat->events.request_set_cursor, requestCursor, this,
                 handleRequestCursor);
  attachListener(&seat->events.request_set_selection, requestSelection, this,
                 handleRequestSelection);
  attachListener(&seat->keyboard_state.events.focus_change, keyboardFocusChange,
                 this, handleKeyboardFocusChange);
  attachListener(&cursor->events.motion, cursorMotion, this,
                 handleCursorMotion);
  attachListener(&cursor->events.motion_absolute, cursorMotionAbsolute, this,
                 handleCursorMotionAbsolute);
  attachListener(&cursor->events.button, cursorButton, this,
                 handleCursorButton);
  attachListener(&cursor->events.axis, cursorAxis, this, handleCursorAxis);
  attachListener(&cursor->events.frame, cursorFrame, this, handleCursorFrame);
  attachListener(WlrootsCompat::newInputMethodSignal(inputMethodManager),
                 newInputMethod, this, handleNewInputMethod);
  attachListener(WlrootsCompat::newTextInputSignal(textInputManager),
                 newTextInput, this, handleNewTextInput);
  attachListener(&virtualKeyboardManager->events.new_virtual_keyboard,
                 newVirtualKeyboard, this, handleNewVirtualKeyboard);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  attachListener(&foreignToplevelCaptureSource->events.new_request,
                 foreignToplevelCaptureRequest, this,
                 handleForeignToplevelCaptureRequest);
#endif

  wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER |
                                      WL_SEAT_CAPABILITY_KEYBOARD);

  if (wl_display_add_socket(display, socketName.c_str()) != 0)
    return fail("Could not create the requested Wayland socket.");

  // A few host-side Wayland helpers (notably GPU Screen Recorder's Flatpak
  // bridge) still probe the conventional $XDG_RUNTIME_DIR/wayland-0 path
  // instead of honoring a desktop-specific WAYLAND_DISPLAY. Keep LunaDash's
  // stable lunadash-* socket for its own clients/control paths, but expose the
  // same wl_display through the conventional socket in a real login session.
  // Never add this alias while nested: wayland-0 may belong to the host
  // compositor and must not be shadowed or treated as a startup failure.
  if (!nested && socketName != "wayland-0") {
    if (wl_display_add_socket(display, "wayland-0") == 0) {
      logInfo("LunaDash Wayland compatibility socket: wayland-0");
    } else {
      logWarning("LunaDash could not expose wayland-0 compatibility socket; "
               "helpers that hard-code wayland-0 may not work.");
    }
  }

  if (!wlr_backend_start(backend))
    return fail("wlroots could not start the backend.");

  updateBackground();
  logInfo("LunaDash wlroots socket: %s seat protocol >= %d",
          socketName.c_str(),
          static_cast<int>(WlrootsCompat::expectedSeatProtocolVersion()));
  return true;
}

bool WaylandCompositor::Impl::fail(const char *message) {
  logCritical("LunaDash wlroots: %s", message);
  q->processFailure_ = true;
  terminateEventLoop(2);
  return false;
}

int WaylandCompositor::Impl::runEventLoop(bool profile) {
  if (!display || !eventLoop)
    return 2;
  const int eventLoopFd = wl_event_loop_get_fd(eventLoop);
  if (eventLoopFd < 0)
    return 2;
  // Desktop services use GIO (file monitors, D-Bus and async I/O). Drive the
  // default GLib context from the same poll so those sources run on the
  // compositor thread without a second event loop or toolkit dispatcher.
  GMainContext *context = g_main_context_default();
  const bool ownsContext = g_main_context_acquire(context);
  std::vector<GPollFD> glibDescriptors(16);
  std::vector<pollfd> descriptors;
  while (eventLoopRunning) {
    const auto started = std::chrono::steady_clock::now();
    if (wl_event_loop_dispatch(eventLoop, 0) < 0) {
      logCritical("LunaDash wlroots event dispatch failed.");
      q->processFailure_ = true;
      exitCode = 2;
      break;
    }
    wl_display_flush_clients(display);
    ++dispatchCalls;
    lastDispatchUsec =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started)
            .count();
    maxDispatchUsec = std::max(maxDispatchUsec, lastDispatchUsec);
    if (lastDispatchUsec >= 8000)
      ++slowDispatches;
    if (profile && lastDispatchUsec > 25000)
      logWarning("LunaDash event-loop dispatch: %.3f ms",
                 static_cast<double>(lastDispatchUsec) / 1000.0);
    if (!eventLoopRunning)
      break;

    gint maxPriority = 0;
    gint timeout = -1;
    gint glibCount = 0;
    if (ownsContext) {
      g_main_context_prepare(context, &maxPriority);
      while ((glibCount = g_main_context_query(
                  context, maxPriority, &timeout, glibDescriptors.data(),
                  static_cast<gint>(glibDescriptors.size()))) >
             static_cast<gint>(glibDescriptors.size()))
        glibDescriptors.resize(static_cast<std::size_t>(glibCount));
    }
    descriptors.clear();
    descriptors.push_back({eventLoopFd, POLLIN, 0});
    for (gint index = 0; index < glibCount; ++index)
      descriptors.push_back({glibDescriptors[static_cast<std::size_t>(index)].fd,
                             static_cast<short>(
                                 glibDescriptors[static_cast<std::size_t>(index)]
                                     .events),
                             0});
    int result;
    do {
      result = ::poll(descriptors.data(), descriptors.size(), timeout);
    } while (result < 0 && errno == EINTR);
    if (result < 0 ||
        (descriptors.front().revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      logCritical("LunaDash could not poll the wlroots event loop.");
      q->processFailure_ = true;
      exitCode = 2;
      break;
    }
    if (ownsContext) {
      for (gint index = 0; index < glibCount; ++index)
        glibDescriptors[static_cast<std::size_t>(index)].revents =
            static_cast<gushort>(
                descriptors[static_cast<std::size_t>(index) + 1].revents);
      if (g_main_context_check(context, maxPriority, glibDescriptors.data(),
                               glibCount))
        g_main_context_dispatch(context);
    }
  }
  if (ownsContext)
    g_main_context_release(context);
  return exitCode;
}

void WaylandCompositor::Impl::terminateEventLoop(int status) {
  exitCode = status;
  eventLoopRunning = false;
}

bool WaylandCompositor::Impl::scheduleScratchpadTimeout(std::uint64_t generation) {
  cancelScratchpadTimeout();
  if (!eventLoop)
    return false;
  scratchpadTimeout =
      wl_event_loop_add_timer(eventLoop, handleScratchpadTimeout, this);
  if (!scratchpadTimeout) {
    logCritical("Could not create the scratchpad launch timer.");
    return false;
  }
  scratchpadTimeoutGeneration = generation;
  if (wl_event_source_timer_update(scratchpadTimeout, 15000) < 0) {
    cancelScratchpadTimeout();
    logCritical("Could not schedule the scratchpad launch timer.");
    return false;
  }
  return true;
}

void WaylandCompositor::Impl::cancelScratchpadTimeout() {
  if (!scratchpadTimeout)
    return;
  wl_event_source_remove(scratchpadTimeout);
  scratchpadTimeout = nullptr;
}

int WaylandCompositor::Impl::handleScratchpadTimeout(void *data) {
  auto *self = static_cast<Impl *>(data);
  if (!self)
    return 0;
  self->cancelScratchpadTimeout();
  auto *owner = self->q;
  if (owner && owner->scratchpadPending_ &&
      owner->scratchpadGeneration_ == self->scratchpadTimeoutGeneration) {
    owner->scratchpadPending_ = false;
    owner->scratchpadProcess_ = 0;
    owner->scratchpadError_ =
        "The terminal did not create a new window. Configure "
        "the terminal role to start a separate process.";
  }
  return 0;
}

bool WaylandCompositor::Impl::scheduleCallback(
    int delayMilliseconds, std::function<void()> callback) {
  if (!eventLoop || !callback)
    return false;
  auto scheduled = std::make_unique<ScheduledCallback>();
  scheduled->impl = this;
  scheduled->callback = std::move(callback);
  auto *entry = scheduled.get();
  scheduledCallbacks.push_back(std::move(scheduled));
  entry->source =
      wl_event_loop_add_timer(eventLoop, handleScheduledCallback, entry);
  if (!entry->source ||
      // A zero delay disarms a wl_event_loop timer; dispatch "now" as 1 ms.
      wl_event_source_timer_update(entry->source,
                                   std::max(1, delayMilliseconds)) < 0) {
    if (entry->source)
      wl_event_source_remove(entry->source);
    const auto found = std::find_if(
        scheduledCallbacks.begin(), scheduledCallbacks.end(),
        [entry](const auto &pending) { return pending.get() == entry; });
    if (found != scheduledCallbacks.end())
      scheduledCallbacks.erase(found);
    logCritical("Could not schedule a compositor event-loop callback.");
    return false;
  }
  return true;
}

void WaylandCompositor::Impl::cancelScheduledCallbacks() {
  for (const auto &entry : scheduledCallbacks)
    if (entry && entry->source)
      wl_event_source_remove(entry->source);
  scheduledCallbacks.clear();
}

int WaylandCompositor::Impl::handleScheduledCallback(void *data) {
  auto *entry = static_cast<ScheduledCallback *>(data);
  if (!entry || !entry->impl)
    return 0;
  auto *self = entry->impl;
  auto callback = std::move(entry->callback);
  const auto found =
      std::find_if(self->scheduledCallbacks.begin(),
                   self->scheduledCallbacks.end(),
                   [entry](const auto &pending) {
                     return pending.get() == entry;
                   });
  if (found != self->scheduledCallbacks.end()) {
    if (entry->source)
      wl_event_source_remove(entry->source);
    self->scheduledCallbacks.erase(found);
  }
  if (callback)
    callback();
  return 0;
}

void WaylandCompositor::Impl::shutdown() {
  releaseRuntime();
  if (!display)
    return;
  wl_display_destroy(display);
  display = nullptr;
  eventLoop = nullptr;
}

void WaylandCompositor::Impl::releaseRuntime() {
  cancelScheduledCallbacks();
  cancelScratchpadTimeout();
  clearPendingDisplay();
  if (!display || runtimeReleased)
    return;
  runtimeReleased = true;

  // Underlays retain renderer buffers and listeners on client scene nodes.
  // Release them while both the scene and renderer are still alive.
  windowGlass.reset();
  windowCorners.reset();
  wl_display_destroy_clients(display);

  detachListener(newOutput);
  detachListener(newInput);
  detachListener(newXdgSurface);
  detachListener(newXdgPopup);
  detachListener(newLayerSurface);
#if LUDASH_WLR_HAS_XDG_ACTIVATION
  detachListener(requestXdgActivation);
#endif
  detachListener(requestCursor);
  detachListener(requestSelection);
  detachListener(keyboardFocusChange);
  detachListener(cursorMotion);
  detachListener(cursorMotionAbsolute);
  detachListener(cursorButton);
  detachListener(cursorAxis);
  detachListener(cursorFrame);
  detachListener(newInputMethod);
  detachListener(newTextInput);
  detachListener(newVirtualKeyboard);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  detachListener(foreignToplevelCaptureRequest);
#endif

  // Runtime wrappers keep listeners on wlroots-owned objects. Disconnect
  // every wrapper before destroying the scene/backend so late output/input
  // destruction cannot call back into already-freed scene nodes.
  const auto outputStates = outputs;
  outputs.clear();
  primaryOutput = nullptr;
  for (auto *state : outputStates) {
    if (!state)
      continue;
    detachListener(state->frame);
    detachListener(state->destroy);
    detachListener(state->requestState);
    ludash_night_color_destroy(state->nightColor);
    delete state;
  }

  const auto xwaylandStates = xwaylandSurfaces;
  xwaylandSurfaces.clear();
  for (auto *state : xwaylandStates) {
    if (!state)
      continue;
    detachListener(state->associate);
    detachListener(state->dissociate);
    detachListener(state->sceneDestroy);
    detachListener(state->sceneContentDestroy);
    detachListener(state->map);
    detachListener(state->unmap);
    detachListener(state->destroy);
    detachListener(state->requestConfigure);
    detachListener(state->requestMove);
    detachListener(state->requestResize);
    detachListener(state->requestMinimize);
    detachListener(state->requestMaximize);
    detachListener(state->requestFullscreen);
    detachListener(state->requestActivate);
    detachListener(state->setTitle);
    detachListener(state->setClass);
    detachListener(state->setParent);
    detachListener(state->setGeometry);
    detachListener(state->setOverrideRedirect);
    delete state;
  }

  const auto layerStates = layers;
  layers.clear();
  for (auto *state : layerStates) {
    if (!state)
      continue;
    detachListener(state->map);
    detachListener(state->unmap);
    detachListener(state->commit);
    detachListener(state->destroy);
    delete state;
  }

  const auto keyboardStates = keyboards;
  keyboards.clear();
  for (auto *state : keyboardStates) {
    if (!state)
      continue;
    detachListener(state->key);
    detachListener(state->keymap);
    detachListener(state->modifiers);
    detachListener(state->destroy);
    delete state;
  }

  const auto textInputStates = textInputs;
  textInputs.clear();
  activeTextInput = nullptr;
  for (auto *state : textInputStates) {
    if (!state)
      continue;
    detachListener(state->enable);
    detachListener(state->commit);
    detachListener(state->disable);
    detachListener(state->destroy);
    delete state;
  }

  const auto popupStates = inputPopups;
  inputPopups.clear();
  for (auto *state : popupStates) {
    if (!state)
      continue;
    detachListener(state->destroy);
    delete state;
  }

  if (inputMethod) {
    detachListener(inputMethod->commit);
    detachListener(inputMethod->newPopup);
    detachListener(inputMethod->grabKeyboard);
    detachListener(inputMethod->destroy);
    delete inputMethod;
    inputMethod = nullptr;
  }

  if (scene) {
    wlr_scene_node_destroy(&scene->tree.node);
    scene = nullptr;
  }
  sceneLayout = nullptr;
  backgroundLayer = nullptr;
  bottomLayer = nullptr;
  normalLayer = nullptr;
  animationLayer = nullptr;
  topLayer = nullptr;
  fullscreenLayer = nullptr;
  overlayLayer = nullptr;
  background = nullptr;

  if (cursorManager) {
    wlr_xcursor_manager_destroy(cursorManager);
    cursorManager = nullptr;
  }
  WlrootsCompat::destroyRuntimeObjects(cursor, allocator, renderer, backend);
  cursor = nullptr;
  allocator = nullptr;
  renderer = nullptr;
  backend = nullptr;
  compositor = nullptr;
  if (outputLayout) {
    wlr_output_layout_destroy(outputLayout);
    outputLayout = nullptr;
  }
}

bool WaylandCompositor::Impl::inputBridgeReady() const {
  return inputMethodManager && textInputManager && virtualKeyboardManager;
}
} // namespace LunaDash
