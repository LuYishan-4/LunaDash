#pragma once

struct wl_display;
struct wlr_idle_inhibit_manager_v1;
struct wlr_idle_notifier_v1;
struct wlr_seat;

namespace LunaDash {

// ext-idle-notify-v1: tells clients (screen lockers, the shell's idle timers)
// when the user has been inactive. Input reports activity; while any client
// holds an idle inhibitor (a playing video, a presentation) the session never
// becomes idle.
class IdleNotify final {
public:
  IdleNotify(wl_display *display, wlr_idle_inhibit_manager_v1 *inhibit,
             wlr_seat *seat);
  ~IdleNotify();
  IdleNotify(const IdleNotify &) = delete;
  IdleNotify &operator=(const IdleNotify &) = delete;

  bool valid() const { return notifier_ != nullptr; }
  // Any user input: keys, pointer motion, buttons, scrolling.
  void activity();
  int inhibitors() const { return inhibitors_; }

  struct Listeners;

private:
  void inhibitorAdded(void *inhibitor);
  void inhibitorRemoved();

  wlr_idle_notifier_v1 *notifier_ = nullptr;
  wlr_seat *seat_ = nullptr;
  int inhibitors_ = 0;
  Listeners *listeners_ = nullptr;
};

} // namespace LunaDash
