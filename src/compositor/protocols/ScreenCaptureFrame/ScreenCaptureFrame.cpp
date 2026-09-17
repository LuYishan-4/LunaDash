#include "wlr-screencopy-server.h"
#include "compositor/protocols/ScreenCaptureFrame/ScreenCaptureFrame.hpp"
#include <cstdint>
#include <cstring>
#include <ctime>
#include <wayland-server-core.h>
namespace LuDash {
ScreenCaptureFrame::ScreenCaptureFrame(ScreenCapture *capture,
                                       wl_resource *resource,
                                       const QRect &region)
    : QObject(capture), capture_(capture), resource_(resource),
      region_(region) {
  // Requests are dispatched in the order the protocol declares them:
  // copy, destroy, then the since="2" copy_with_damage.
  static const struct zwlr_screencopy_frame_v1_interface implementation = {
      copy, destroy, copyWithDamage};
  wl_resource_set_implementation(resource, &implementation, this,
                                 resourceDestroyed);
  capture_->frames_ << this;
  if (region_.isEmpty()) {
    zwlr_screencopy_frame_v1_send_failed(resource_);
    return;
  }
  const auto width = static_cast<uint32_t>(region_.width()),
             height = static_cast<uint32_t>(region_.height());
  zwlr_screencopy_frame_v1_send_buffer(resource_, WL_SHM_FORMAT_XRGB8888, width,
                                       height, width * 4u);
  if (wl_resource_get_version(resource_) >= 3)
    zwlr_screencopy_frame_v1_send_buffer_done(resource_);
}
ScreenCaptureFrame::~ScreenCaptureFrame() {
  if (capture_)
    capture_->frames_.removeAll(this);
  if (resource_) {
    wl_resource_set_user_data(resource_, nullptr);
    wl_resource_destroy(resource_);
  }
}
ScreenCaptureFrame *ScreenCaptureFrame::get(wl_resource *resource) {
  return static_cast<ScreenCaptureFrame *>(wl_resource_get_user_data(resource));
}
void ScreenCaptureFrame::copy(wl_client *, wl_resource *resource,
                              wl_resource *buffer) {
  auto *frame = get(resource);
  if (!frame)
    return;
  if (frame->used_) {
    wl_resource_post_error(resource,
                           ZWLR_SCREENCOPY_FRAME_V1_ERROR_ALREADY_USED,
                           "Frame was already used to copy a buffer");
    return;
  }
  frame->capture(buffer, false);
}
void ScreenCaptureFrame::copyWithDamage(wl_client *, wl_resource *resource,
                                        wl_resource *buffer) {
  auto *frame = get(resource);
  if (!frame)
    return;
  if (frame->used_) {
    wl_resource_post_error(resource,
                           ZWLR_SCREENCOPY_FRAME_V1_ERROR_ALREADY_USED,
                           "Frame was already used to copy a buffer");
    return;
  }
  frame->capture(buffer, true);
}
void ScreenCaptureFrame::destroy(wl_client *, wl_resource *resource) {
  wl_resource_destroy(resource);
}
void ScreenCaptureFrame::resourceDestroyed(wl_resource *resource) {
  if (auto *frame = get(resource)) {
    frame->resource_ = nullptr;
    delete frame;
  }
}
void ScreenCaptureFrame::capture(wl_resource *buffer, bool withDamage) {
  if (!resource_ || !capture_)
    return;
  used_ = true;
  auto *shm = buffer ? wl_shm_buffer_get(buffer) : nullptr;
  if (!shm) {
    wl_resource_post_error(resource_,
                           ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
                           "Only wl_shm buffers are supported");
    return;
  }
  const auto format = wl_shm_buffer_get_format(shm);
  if (format != WL_SHM_FORMAT_XRGB8888 && format != WL_SHM_FORMAT_ARGB8888) {
    wl_resource_post_error(resource_,
                           ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
                           "Unsupported buffer format");
    return;
  }
  if (wl_shm_buffer_get_width(shm) != region_.width() ||
      wl_shm_buffer_get_height(shm) != region_.height()) {
    wl_resource_post_error(
        resource_, ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
        "Buffer size does not match the announced frame size");
    return;
  }
  const int stride = wl_shm_buffer_get_stride(shm);
  if (stride < region_.width() * 4) {
    wl_resource_post_error(resource_,
                           ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
                           "Buffer stride is too small");
    return;
  }
  const QImage frame = capture_->grabRegion(region_);
  if (frame.size() != region_.size()) {
    zwlr_screencopy_frame_v1_send_failed(resource_);
    return;
  }
  // wl_shm buffers hold little-endian XRGB8888, which is byte-for-byte the
  // layout of QImage::Format_RGB32 on a little-endian host.
  const QImage source = frame.convertToFormat(QImage::Format_RGB32);
  wl_shm_buffer_begin_access(shm);
  auto *data = static_cast<uint8_t *>(wl_shm_buffer_get_data(shm));
  if (data) {
    const auto bytes = static_cast<size_t>(region_.width()) * 4u;
    for (int y = 0; y < region_.height(); ++y)
      std::memcpy(data + static_cast<size_t>(y) * static_cast<size_t>(stride),
                  source.constScanLine(y), bytes);
  }
  wl_shm_buffer_end_access(shm);
  if (!data) {
    zwlr_screencopy_frame_v1_send_failed(resource_);
    return;
  }
  // The frame is copied from the current content, so the whole region is the
  // damage since the previous copy.
  if (withDamage)
    zwlr_screencopy_frame_v1_send_damage(
        resource_, 0, 0, static_cast<uint32_t>(region_.width()),
        static_cast<uint32_t>(region_.height()));
  zwlr_screencopy_frame_v1_send_flags(resource_, 0);
  sendReady();
  if (capture_)
    ++capture_->capturedFrames_;
}
void ScreenCaptureFrame::sendReady() {
  timespec now = {};
  clock_gettime(CLOCK_MONOTONIC, &now);
  const auto seconds = static_cast<uint64_t>(now.tv_sec);
  zwlr_screencopy_frame_v1_send_ready(
      resource_, static_cast<uint32_t>(seconds >> 32),
      static_cast<uint32_t>(seconds & 0xffffffffu),
      static_cast<uint32_t>(now.tv_nsec));
}
} // namespace LuDash
