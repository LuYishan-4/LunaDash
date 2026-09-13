#pragma once
class QGuiApplication;
namespace LuDash {
// Construct immediately after the application, before windows and other UI objects.
// The guard runs after their destruction and before Qt joins its Wayland readers.
class WaylandClientShutdown final {
public:
    explicit WaylandClientShutdown(QGuiApplication& application);
    ~WaylandClientShutdown();
    WaylandClientShutdown(const WaylandClientShutdown&) = delete;
    WaylandClientShutdown& operator=(const WaylandClientShutdown&) = delete;
private:
    QGuiApplication& application_;
};
}
