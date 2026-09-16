#pragma once
#include <LuDash/screen_capture/ScreenCapture.h>
namespace LuDash {
// One zwlr_screencopy_frame_v1 object. The client allocates a wl_shm buffer
// with the announced parameters and asks for a copy of the frame's region.
class ScreenCaptureFrame final : public QObject {
public:
  ScreenCaptureFrame(ScreenCapture *capture, wl_resource *resource,
                     const QRect &region);
  ~ScreenCaptureFrame() override;

private:
  friend class ScreenCapture;
  ScreenCapture *capture_;
  wl_resource *resource_;
  QRect region_;
  bool used_ = false;
  static ScreenCaptureFrame *get(wl_resource *resource);
  static void copy(wl_client *client, wl_resource *resource,
                   wl_resource *buffer);
  static void destroy(wl_client *client, wl_resource *resource);
  static void copyWithDamage(wl_client *client, wl_resource *resource,
                             wl_resource *buffer);
  static void resourceDestroyed(wl_resource *resource);
  void capture(wl_resource *buffer, bool withDamage);
  void sendReady();
};
} // namespace LuDash
