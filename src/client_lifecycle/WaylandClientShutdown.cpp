#include <LuDash/client_lifecycle/WaylandClientShutdown.h>
#include <QGuiApplication>
#include <QDeadlineTimer>
#include <QDebug>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtGui/qguiapplication_platform.h>
#if QT_CONFIG(wayland)
#include <wayland-client.h>
#include <poll.h>
#include <cerrno>
#endif
#endif
namespace LuDash {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#if QT_CONFIG(wayland)
namespace {
void syncFinished(void* data, wl_callback*, uint32_t) { *static_cast<bool*>(data) = true; }
bool finishReadCycle(wl_display* display) {
    auto* queue = wl_display_create_queue(display);
    if (!queue) return false;
    auto* wrapper = static_cast<wl_display*>(wl_proxy_create_wrapper(display));
    if (!wrapper) { wl_event_queue_destroy(queue); return false; }
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapper), queue);
    auto* callback = wl_display_sync(wrapper);
    wl_proxy_wrapper_destroy(wrapper);
    if (!callback) { wl_event_queue_destroy(queue); return false; }
    bool done = false;
    const wl_callback_listener listener{syncFinished};
    wl_callback_add_listener(callback, &listener, &done);
    QDeadlineTimer deadline(1000);
    while (!done && !deadline.hasExpired() && wl_display_get_error(display) == 0) {
        if (wl_display_prepare_read_queue(display, queue) != 0) {
            if (wl_display_dispatch_queue_pending(display, queue) < 0) break;
            continue;
        }
        const bool blocked = wl_display_flush(display) < 0;
        if (blocked && errno != EAGAIN) { wl_display_cancel_read(display); break; }
        pollfd descriptor{wl_display_get_fd(display), static_cast<short>(POLLIN | (blocked ? POLLOUT : 0)), 0};
        const int result = poll(&descriptor, 1, static_cast<int>(deadline.remainingTime()));
        const int pollError = errno;
        if (result > 0 && (descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
            if (wl_display_read_events(display) < 0) break;
        } else {
            wl_display_cancel_read(display);
            if (result == 0 || (result < 0 && pollError != EINTR)) break;
        }
        if (wl_display_dispatch_queue_pending(display, queue) < 0) break;
    }
    wl_callback_destroy(callback);
    wl_event_queue_destroy(queue);
    return done;
}
}
#endif
#endif
WaylandClientShutdown::WaylandClientShutdown(QGuiApplication& application) : application_(application) {}
WaylandClientShutdown::~WaylandClientShutdown() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#if QT_CONFIG(wayland)
    if (application_.platformName() != QStringLiteral("wayland")) return;
    auto* native = application_.nativeInterface<QNativeInterface::QWaylandApplication>();
    auto* display = native ? native->display() : nullptr;
    // Windows are destroyed and the GUI event loop has returned. A private sync
    // completes any prepared read without dispatching Qt's default event queue.
    // Its GUI-dispatched reader then waits idle while Qt joins both readers.
    if (display && !finishReadCycle(display))
        qWarning("Wayland shutdown synchronization did not complete.");
#endif
#endif
}
}
