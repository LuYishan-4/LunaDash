#include "compositor/wayland/Register.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTimer>

namespace LunaDash {
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

WaylandCompositor::Impl::Impl(WaylandCompositor *owner,
                              const QByteArray &socket, bool wantsFullscreen,
                              const QString &renderer)
    : q(owner), socketName(socket), fullscreen(wantsFullscreen),
      nested(!qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty() ||
             !qEnvironmentVariable("DISPLAY").isEmpty()),
      rendererPreference(renderer) {}

WaylandCompositor::Impl::~Impl() { shutdown(); }

bool WaylandCompositor::Impl::initialize() {
  if (rendererPreference == "opengl" || rendererPreference == "gles") {
    if (qEnvironmentVariableIsEmpty("WLR_RENDERER"))
      qputenv("WLR_RENDERER", "gles2");
  }

  wlr_log_init(WLR_INFO, nullptr);
  display = wl_display_create();
  if (!display)
    return fail("Could not create the Wayland display.");
  eventLoop = wl_display_get_event_loop(display);

  backend = WlrootsCompat::createBackend(display);
  if (!backend)
    return fail("wlroots could not create a backend.");
  renderer = wlr_renderer_autocreate(backend);
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

  wlr_compositor_create(display, 5, renderer);
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
#if LUDASH_WLR_HAS_DRM_SYNCOBJ
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
      qWarning("LunaDash: DRM explicit synchronization is unavailable.");
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

  xdgShell = wlr_xdg_shell_create(display, 3);
  layerShell = wlr_layer_shell_v1_create(display, 4);
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
      nullptr, qEnvironmentVariableIntValue("XCURSOR_SIZE") > 0
                   ? qEnvironmentVariableIntValue("XCURSOR_SIZE")
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

  if (wl_display_add_socket(display, socketName.constData()) != 0)
    return fail("Could not create the requested Wayland socket.");

  const int fd = wl_event_loop_get_fd(eventLoop);
  waylandNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, q);
  QObject::connect(waylandNotifier, &QSocketNotifier::activated, q,
                   [this] { dispatch(); });
  if (auto *dispatcher = QAbstractEventDispatcher::instance()) {
    QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, q,
                     [this] { dispatch(); });
  }

  if (!wlr_backend_start(backend))
    return fail("wlroots could not start the backend.");

  updateBackground();
  qInfo().noquote() << "LunaDash wlroots socket:" << socketName
                    << "seat protocol >="
                    << WlrootsCompat::expectedSeatProtocolVersion();
  return true;
}

bool WaylandCompositor::Impl::fail(const char *message) {
  qCritical().noquote() << "LunaDash wlroots:" << message;
  q->processFailure_ = true;
  QTimer::singleShot(0, q, [] { QCoreApplication::exit(2); });
  return false;
}

void WaylandCompositor::Impl::dispatch() {
  if (!eventLoop)
    return;
  if (wl_event_loop_dispatch(eventLoop, 0) < 0) {
    q->processFailure_ = true;
    qCritical("LunaDash wlroots event dispatch failed.");
    QCoreApplication::exit(2);
    return;
  }
  if (display)
    wl_display_flush_clients(display);
}

void WaylandCompositor::Impl::shutdown() {
  clearPendingDisplay();
  if (!display)
    return;
  if (waylandNotifier) {
    waylandNotifier->setEnabled(false);
    waylandNotifier->deleteLater();
    waylandNotifier = nullptr;
  }

  wl_display_destroy_clients(display);

  detachListener(newOutput);
  detachListener(newInput);
  detachListener(newXdgSurface);
  detachListener(newXdgPopup);
  detachListener(newLayerSurface);
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
  if (outputLayout) {
    wlr_output_layout_destroy(outputLayout);
    outputLayout = nullptr;
  }
  wl_display_destroy(display);
  display = nullptr;
  eventLoop = nullptr;
}

bool WaylandCompositor::Impl::inputBridgeReady() const {
  return inputMethodManager && textInputManager && virtualKeyboardManager;
}
} // namespace LunaDash
