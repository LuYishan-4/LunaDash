#pragma once

#include "core/templates/LunaClint.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <sys/types.h>

struct wl_display;
struct wl_event_loop;
struct wl_event_source;
struct wl_listener;
struct wlr_compositor;
struct wlr_seat;
struct wlr_xwayland;
struct wlr_xwayland_surface;
struct wlr_xcursor_manager;

namespace LunaDash {

struct XWaylandSnapshot {
  static constexpr const char *mode = "rootless-lazy-xwm";
  bool available = false;
  bool running = false;
  bool selectionBridge = false;
  std::string display;
  std::string error;
};

class XWaylandSupport final : public Templates::LunaClint<XWaylandSupport> {
public:
  using NewSurfaceCallback = std::function<void(wlr_xwayland_surface *)>;

  XWaylandSupport() = default;
  ~XWaylandSupport();
  XWaylandSupport(const XWaylandSupport &) = delete;
  XWaylandSupport &operator=(const XWaylandSupport &) = delete;

  bool start(wl_display *display, wlr_compositor *compositor, wlr_seat *seat,
             wlr_xcursor_manager *cursorManager,
             std::map<std::string, std::string> environment,
             NewSurfaceCallback newSurface);
  void stop();
  bool stopped() const;
  bool startServer(std::string *error = nullptr);
  XWaylandSnapshot snapshot() const;
  void applyEnvironment(std::map<std::string, std::string> &environment) const;
  bool launch(const std::vector<std::string> &command, std::string *error);

private:
  using Slot = Templates::WaylandSlot;
  void onReady();
  void onNewSurface(wlr_xwayland_surface *surface);
  // Collects exited X11 helper processes; runs from the event loop.
  void reapClients();
  bool ensureClientReaper();

  wlr_xwayland *xwayland_ = nullptr;
  wlr_seat *seat_ = nullptr;
  wlr_xcursor_manager *cursorManager_ = nullptr;
  wl_event_loop *eventLoop_ = nullptr;
  wl_event_source *clientReaper_ = nullptr;
  std::vector<pid_t> clients_;
  std::map<std::string, std::string> environment_;
  NewSurfaceCallback newSurface_;
  Slot ready_;
  Slot newSurfaceListener_;
  std::string error_;
  bool xwmReady_ = false;
};

// Replaces DISPLAY/XAUTHORITY in a client environment with the running
// XWayland server's values, or removes them without XWayland.
void applyXWaylandEnvironment(std::map<std::string, std::string> &environment,
                              const XWaylandSupport *support);

} // namespace LunaDash
