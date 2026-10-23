#pragma once

#include "core/templates/WaylandSlot.hpp"

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

namespace LunaDash {

struct XWaylandSnapshot {
  bool available = false;
  bool running = false;
  bool selectionBridge = false;
  std::string display;
  std::string error;
};

class XWaylandSupport final {
public:
  using NewSurfaceCallback = std::function<void(wlr_xwayland_surface *)>;

  XWaylandSupport() = default;
  ~XWaylandSupport();
  XWaylandSupport(const XWaylandSupport &) = delete;
  XWaylandSupport &operator=(const XWaylandSupport &) = delete;

  bool start(wl_display *display, wlr_compositor *compositor, wlr_seat *seat,
             std::map<std::string, std::string> environment,
             NewSurfaceCallback newSurface);
  void stop();
  bool stopped() const;
  bool startServer(std::string *error = nullptr);
  XWaylandSnapshot snapshot() const;
  void applyEnvironment(std::map<std::string, std::string> &environment) const;
  bool launch(const std::vector<std::string> &command, std::string *error);

private:
  using Slot = Templates::WaylandSlot<XWaylandSupport>;
  static void handleReady(wl_listener *listener, void *data);
  static void handleNewSurface(wl_listener *listener, void *data);
  static int handleClientReaper(void *data);
  void reapClients();
  bool ensureClientReaper();

  wlr_xwayland *xwayland_ = nullptr;
  wlr_seat *seat_ = nullptr;
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

} // namespace LunaDash
