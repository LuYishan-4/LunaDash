#include "compositor/wayland/Register.hpp"
#include "compositor/renderer/selection/RenderSelection.h"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/log/Log.hpp"

namespace LunaDash {

WaylandCompositor::Impl::Impl(WaylandCompositor *owner)
    : q(owner), nested(!environmentValue("WAYLAND_DISPLAY").empty() ||
                       !environmentValue("DISPLAY").empty()) {
  loadOutputSizePreferences();
  usableArea = Rect(0, 0, virtualOutputSize.width(), virtualOutputSize.height());
  display = wl_display_create();
  if (display)
    eventLoop = wl_display_get_event_loop(display);
}

WaylandCompositor::Impl::~Impl() { shutdown(); }

bool WaylandCompositor::Impl::createRuntime() {
  if (!display || !eventLoop)
    return fail("Could not create the Wayland display.");

  backend = WlrootsCompat::createPlatform(display);
  if (!backend)
    return fail("wlroots could not create a backend.");
  renderer = ludash_renderer_create(backend, options().graphics.c_str());
  if (!renderer)
    return fail("wlroots could not create a renderer.");
#if WLR_VERSION_MINOR >= 20
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
  wlr_viewporter_create(display);
  // The scene sends each surface its fractional scale; clients render sharply at 125 %.
  fractionalScale = wlr_fractional_scale_manager_v1_create(display, 1);
  if (!fractionalScale)
    return fail("wlroots could not create the fractional-scale global.");
#if WLR_VERSION_MINOR >= 20
  if (!wlr_single_pixel_buffer_manager_v1_create(display))
    return fail("wlroots could not create the single-pixel buffer global.");
#endif
  idleInhibit = wlr_idle_inhibit_v1_create(display);
  if (!idleInhibit)
    return fail("wlroots could not create the idle-inhibit global.");

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
  overviewLayer = wlr_scene_tree_create(&scene->tree);
  topLayer = wlr_scene_tree_create(&scene->tree);
  // True xdg fullscreen sits above ordinary top-layer panels. Layer-shell
  // OVERLAY stays above it for OSD/recorder overlays and selectors.
  fullscreenLayer = wlr_scene_tree_create(&scene->tree);
  overlayLayer = wlr_scene_tree_create(&scene->tree);
  background = wlr_scene_rect_create(backgroundLayer, virtualOutputSize.width(),
                                     virtualOutputSize.height(), backgroundColor);
  windowGlass = std::make_unique<WindowGlass>(
      renderer, allocator, &scene->tree.node, &normalLayer->node, eventLoop);
  windowGlass->setFrameRequest([this] {
    for (const auto *state : outputs)
      if (state && state->output)
        wlr_output_schedule_frame(state->output);
  });
  windowCorners = std::make_unique<WindowCorners>();

  seat = wlr_seat_create(display, "seat0");
  cursor = wlr_cursor_create();
  const uint32_t cursorSize =
      environmentInt("XCURSOR_SIZE") > 0
          ? static_cast<uint32_t>(environmentInt("XCURSOR_SIZE"))
          : 24;
  cursorManager = wlr_xcursor_manager_create(nullptr, cursorSize);
  if (!seat || !cursor || !cursorManager)
    return fail("wlroots could not create the seat/cursor stack.");
  pointerCursor = std::make_unique<PointerCursor>(
      cursor, cursorManager, cursorSize, [this] {
        for (const auto *state : outputs)
          if (state && state->output)
            wlr_output_schedule_frame(state->output);
      });
  wlr_cursor_attach_output_layout(cursor, outputLayout);
  wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER |
                                      WL_SEAT_CAPABILITY_KEYBOARD);
  return true;
}

bool WaylandCompositor::Impl::startRuntime() {
  if (wl_display_add_socket(display, options().socket.c_str()) != 0)
    return fail("Could not create the requested Wayland socket.");

  // A few host-side Wayland helpers (notably GPU Screen Recorder's Flatpak
  // bridge) still probe the conventional $XDG_RUNTIME_DIR/wayland-0 path
  // instead of honoring a desktop-specific WAYLAND_DISPLAY. Keep LunaDash's
  // stable lunadash-* socket for its own clients/control paths, but expose the
  // same wl_display through the conventional socket in a real login session.
  // Never add this alias while nested: wayland-0 may belong to the host
  // compositor and must not be shadowed or treated as a startup failure.
  if (!nested && options().socket != "wayland-0") {
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
          options().socket.c_str(),
          static_cast<int>(WlrootsCompat::expectedSeatProtocolVersion()));
  return true;
}

void WaylandCompositor::Impl::destroyRuntime() {
  if (scene) {
    wlr_scene_node_destroy(&scene->tree.node);
    scene = nullptr;
  }
  sceneLayout = nullptr;
  backgroundLayer = nullptr;
  bottomLayer = nullptr;
  normalLayer = nullptr;
  animationLayer = nullptr;
  overviewLayer = nullptr;
  topLayer = nullptr;
  fullscreenLayer = nullptr;
  overlayLayer = nullptr;
  background = nullptr;

  pointerCursor.reset();
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
