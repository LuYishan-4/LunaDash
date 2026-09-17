#include "compositor/WaylandClientShutdown/WaylandClientShutdown.hpp"

#include <QDeadlineTimer>
#include <QDebug>
#include <QGuiApplication>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtGui/qguiapplication_platform.h>
#if QT_CONFIG(wayland)
#include <cerrno>
#include <poll.h>
#include <wayland-client.h>
#endif
#endif

namespace LuDash {

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#if QT_CONFIG(wayland)

struct WaylandShutdownHelper {
  wl_event_queue *queue{nullptr};
  wl_callback *callback{nullptr};

  ~WaylandShutdownHelper() {
    if (callback)
      wl_callback_destroy(callback);
    if (queue)
      wl_event_queue_destroy(queue);
  }

  static void syncFinished(void *data, wl_callback *, uint32_t) {
    *static_cast<bool *>(data) = true;
  }
  static bool finishReadCycle(wl_display *display) {
    WaylandShutdownHelper helper;
    helper.queue = wl_display_create_queue(display);
    if (!helper.queue)
      return false;
    auto *wrapper = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
    if (!wrapper)
      return false;

    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapper), helper.queue);
    helper.callback = wl_display_sync(wrapper);
    wl_proxy_wrapper_destroy(wrapper);

    if (!helper.callback)
      return false;

    bool done = false;
    static constexpr wl_callback_listener listener{syncFinished};
    wl_callback_add_listener(helper.callback, &listener, &done);

    QDeadlineTimer deadline(1000);

    while (!done && !deadline.hasExpired() &&
           wl_display_get_error(display) == 0) {
      if (wl_display_prepare_read_queue(display, helper.queue) != 0) {
        if (wl_display_dispatch_queue_pending(display, helper.queue) < 0)
          break;
        continue;
      }

      const bool blocked = (wl_display_flush(display) < 0);
      if (blocked && errno != EAGAIN) {
        wl_display_cancel_read(display);
        break;
      }

      pollfd descriptor{wl_display_get_fd(display),
                        static_cast<short>(POLLIN | (blocked ? POLLOUT : 0)),
                        0};

      const int result =
          poll(&descriptor, 1, static_cast<int>(deadline.remainingTime()));
      const int pollError = errno;

      if (result > 0 && (descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
        if (wl_display_read_events(display) < 0)
          break;
      } else {
        wl_display_cancel_read(display);
        if (result == 0 || (result < 0 && pollError != EINTR))
          break;
      }

      if (wl_display_dispatch_queue_pending(display, helper.queue) < 0)
        break;
    }

    return done;
  }
};

#endif
#endif

WaylandClientShutdown::WaylandClientShutdown(QGuiApplication &application)
    : application_(application) {}

WaylandClientShutdown::~WaylandClientShutdown() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#if QT_CONFIG(wayland)
  if (application_.platformName() != QStringLiteral("wayland"))
    return;

  auto *native =
      application_.nativeInterface<QNativeInterface::QWaylandApplication>();
  auto *display = native ? native->display() : nullptr;

  if (display && !WaylandShutdownHelper::finishReadCycle(display))
    qWarning("Wayland shutdown synchronization did not complete.");
#endif
#endif
}

} // namespace LuDash
