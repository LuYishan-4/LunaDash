#pragma once
class QGuiApplication;
namespace LunaDash {
// Construct immediately after the application, before windows and other UI
// objects. The guard runs after their destruction and before Qt joins its
// Wayland readers.
struct WaylandClientShutdown final {
public:
  explicit WaylandClientShutdown(QGuiApplication &application);
  ~WaylandClientShutdown();
  WaylandClientShutdown(const WaylandClientShutdown &) = delete;
  WaylandClientShutdown &operator=(const WaylandClientShutdown &) = delete;

  WaylandClientShutdown(WaylandClientShutdown &&) = delete;
  WaylandClientShutdown &operator=(WaylandClientShutdown &&) = delete;

private:
  QGuiApplication &application_;
};
} // namespace LunaDash
