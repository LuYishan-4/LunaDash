#include "compositor/wayland/WindowCaptureComponent.hpp"
#include "compositor/client/ClientWindow.hpp"

namespace LunaDash {

bool WaylandCompositor::Impl::WindowCaptureComponent::start() {
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  auto *display = impl->display;
  impl->foreignToplevelList = wlr_ext_foreign_toplevel_list_v1_create(display, 1);
  impl->imageCopyCapture =
      wlr_ext_image_copy_capture_manager_v1_create(display, 1);
  // xdg-desktop-portal-wlr uses this manager for the modern monitor
  // ScreenCast path. Without it, xdpw must fall back to screencopy+dmabuf,
  // which is not available on every renderer/GPU combination.
  if (!wlr_ext_output_image_capture_source_manager_v1_create(display, 1))
    return impl->fail("wlroots could not create output image capture sources.");
  impl->foreignToplevelCaptureSource =
      wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(display, 1);
  if (!impl->foreignToplevelList || !impl->imageCopyCapture ||
      !impl->foreignToplevelCaptureSource)
    return impl->fail(
        "wlroots could not create window capture protocol globals.");
  listen<&WindowCaptureComponent::onCaptureRequest>(
      &impl->foreignToplevelCaptureSource->events.new_request, captureRequest);
#endif
  return true;
}

#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
void WaylandCompositor::Impl::WindowCaptureComponent::onCaptureRequest(
    wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request) {
  if (!request || !request->toplevel_handle)
    return;
  auto *state = static_cast<ToplevelState *>(request->toplevel_handle->data);
  if (!state || state->impl != impl || !state->client->mapped ||
      !state->imageCaptureScene)
    return;
  if (!state->imageCaptureSource)
    state->imageCaptureSource =
        wlr_ext_image_capture_source_v1_create_with_scene_node(
            &state->imageCaptureScene->tree.node, impl->eventLoop,
            impl->allocator, impl->renderer);
  if (state->imageCaptureSource)
    wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
        request, state->imageCaptureSource);
}
#endif

} // namespace LunaDash
