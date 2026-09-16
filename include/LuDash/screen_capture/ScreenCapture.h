#pragma once
#include <QImage>
#include <QList>
#include <QObject>
#include <QRect>
#include <QSize>
#include <cstdint>
class QQuickWindow;
class QWaylandQuickCompositor;
class QWaylandQuickOutput;
struct wl_client;
struct wl_global;
struct wl_resource;
namespace LuDash {
class ScreenCaptureFrame;
// Bind the zwlr_screencopy_manager_v1 global so capture tools can read the
// desktop. LunaDash renders every surface into one window, so a captured frame
// is a region of that window's content. Only the wl_shm path is served; the
// version 3 linux-dmabuf events are deliberately not announced.
class ScreenCapture final : public QObject {
public:
  ScreenCapture(QWaylandQuickCompositor *compositor,
                QWaylandQuickOutput *output, QQuickWindow *window);
  ~ScreenCapture() override;
  int capturedFrames() const { return capturedFrames_; }

private:
  friend class ScreenCaptureFrame;
  QSize outputSize() const;
  QImage grabRegion(const QRect &region) const;
  bool outputSizeFor(wl_resource *managerResource, wl_resource *outputResource,
                     QSize *size) const;
  void createFrame(wl_client *client, wl_resource *managerResource, uint32_t id,
                   const QRect &region);
  QWaylandQuickOutput *output_;
  QQuickWindow *window_;
  wl_global *global_;
  QList<ScreenCaptureFrame *> frames_;
  int capturedFrames_ = 0;
  static void bind(wl_client *client, void *data, uint32_t version,
                   uint32_t id);
  static void captureOutput(wl_client *client, wl_resource *resource,
                            uint32_t id, int32_t overlayCursor,
                            wl_resource *output);
  static void captureOutputRegion(wl_client *client, wl_resource *resource,
                                  uint32_t id, int32_t overlayCursor,
                                  wl_resource *output, int32_t x, int32_t y,
                                  int32_t width, int32_t height);
  static void destroyManager(wl_client *client, wl_resource *resource);
};
} // namespace LuDash
