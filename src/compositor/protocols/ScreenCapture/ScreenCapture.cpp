#include "wlr-screencopy-server.h"
#include "compositor/protocols/ScreenCapture/ScreenCapture.hpp"
#include "compositor/protocols/ScreenCaptureFrame/ScreenCaptureFrame.hpp"
#include <QQuickWindow>
#include <QtWaylandCompositor/QWaylandOutput>
#include <QtWaylandCompositor/QWaylandQuickCompositor>
#include <QtWaylandCompositor/QWaylandQuickOutput>
#include <algorithm>
#include <cstdint>
#include <wayland-server-core.h>
namespace LuDash {
namespace {
// Version 3 is advertised so that current clients see the same generation they
// see on wlroots, but every frame is reported through the wl_shm events. A
// client that knows only version 1 or 2 binds at its own version and is served
// the guaranteed shm buffer event without buffer_done.
constexpr uint32_t kScreenCopyVersion = 3;
} // namespace
ScreenCapture::ScreenCapture(QWaylandQuickCompositor *compositor,
                             QWaylandQuickOutput *output, QQuickWindow *window)
    : QObject(compositor), output_(output), window_(window),
      global_(wl_global_create(
          compositor->display(), &zwlr_screencopy_manager_v1_interface,
          static_cast<int>(kScreenCopyVersion), this, bind)) {}
ScreenCapture::~ScreenCapture() {
  // Frames own their wl_resource and retire themselves; delete the copies the
  // clients left behind before the global disappears.
  while (!frames_.isEmpty())
    delete frames_.last();
  if (global_) {
    wl_global_destroy(global_);
    global_ = nullptr;
  }
}
QSize ScreenCapture::outputSize() const {
  if (window_ && !window_->size().isEmpty())
    return window_->size();
  if (!output_)
    return {};
  const QRect geometry = output_->geometry();
  return geometry.isValid() ? geometry.size() : QSize{};
}
QImage ScreenCapture::grabRegion(const QRect &region) const {
  if (!window_ || region.isEmpty())
    return {};
  QImage frame = window_->grabWindow();
  if (frame.isNull())
    return {};
  // grabWindow() returns physical pixels while the protocol reports logical
  // coordinates, so map the frame back onto the window's logical size before
  // cropping. An integer ratio (a scaled output) stays unblurred.
  const QSize logical = outputSize();
  if (logical.isValid() && !logical.isEmpty() && frame.size() != logical)
    frame =
        frame.scaled(logical, Qt::IgnoreAspectRatio, Qt::FastTransformation);
  return frame.copy(region);
}
bool ScreenCapture::outputSizeFor(wl_resource *managerResource,
                                  wl_resource *outputResource,
                                  QSize *size) const {
  auto *requested =
      outputResource ? QWaylandOutput::fromResource(outputResource) : nullptr;
  if (!requested || requested != output_) {
    wl_resource_post_error(managerResource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                           "Unknown output");
    return false;
  }
  const QSize available = outputSize();
  if (available.isEmpty()) {
    wl_resource_post_error(managerResource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                           "The output has no usable geometry");
    return false;
  }
  *size = available;
  return true;
}
void ScreenCapture::createFrame(wl_client *client, wl_resource *managerResource,
                                uint32_t id, const QRect &region) {
  auto *resource =
      wl_resource_create(client, &zwlr_screencopy_frame_v1_interface,
                         wl_resource_get_version(managerResource), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  new ScreenCaptureFrame(this, resource, region);
}
void ScreenCapture::bind(wl_client *client, void *data, uint32_t version,
                         uint32_t id) {
  static const struct zwlr_screencopy_manager_v1_interface implementation = {
      captureOutput, captureOutputRegion, destroyManager};
  auto *resource = wl_resource_create(
      client, &zwlr_screencopy_manager_v1_interface,
      static_cast<int>(std::min(version, kScreenCopyVersion)), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &implementation, data, nullptr);
}
void ScreenCapture::captureOutput(wl_client *client, wl_resource *resource,
                                  uint32_t id, int32_t overlayCursor,
                                  wl_resource *output) {
  // LunaDash has no separate cursor layer, so a requested cursor overlay
  // cannot be composited; the frame is served without it.
  static_cast<void>(overlayCursor);
  auto *capture =
      static_cast<ScreenCapture *>(wl_resource_get_user_data(resource));
  QSize size;
  if (!capture || !capture->outputSizeFor(resource, output, &size))
    return;
  capture->createFrame(client, resource, id, QRect(QPoint(0, 0), size));
}
void ScreenCapture::captureOutputRegion(wl_client *client,
                                        wl_resource *resource, uint32_t id,
                                        int32_t overlayCursor,
                                        wl_resource *output, int32_t x,
                                        int32_t y, int32_t width,
                                        int32_t height) {
  static_cast<void>(overlayCursor);
  auto *capture =
      static_cast<ScreenCapture *>(wl_resource_get_user_data(resource));
  QSize size;
  if (!capture || !capture->outputSizeFor(resource, output, &size))
    return;
  // The protocol clips the region to the output's extents; an empty result is
  // reported to the client as a failed frame instead of an invalid buffer.
  capture->createFrame(
      client, resource, id,
      QRect(x, y, width, height).intersected(QRect(QPoint(0, 0), size)));
}
void ScreenCapture::destroyManager(wl_client *, wl_resource *resource) {
  wl_resource_destroy(resource);
}
} // namespace LuDash
