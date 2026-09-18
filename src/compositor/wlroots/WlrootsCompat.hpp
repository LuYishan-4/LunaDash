#pragma once

#include "compositor/wlroots/WlrootsHeaders.hpp"


namespace LuDash::WlrootsCompat {

inline wlr_backend *createBackend(wl_display *display) {
#if WLR_VERSION_MINOR < 18
  return wlr_backend_autocreate(display, nullptr);
#else
  return wlr_backend_autocreate(wl_display_get_event_loop(display), nullptr);
#endif
}

inline wlr_output_layout *createOutputLayout(wl_display *display) {
#if WLR_VERSION_MINOR < 18
  (void)display;
  return wlr_output_layout_create();
#else
  return wlr_output_layout_create(display);
#endif
}

inline void notifyPointerAxis(wlr_seat *seat,
                              const wlr_pointer_axis_event *event) {
#if WLR_VERSION_MINOR < 18
  wlr_seat_pointer_notify_axis(seat, event->time_msec, event->orientation,
                               event->delta, event->delta_discrete,
                               event->source);
#else
  wlr_seat_pointer_notify_axis(seat, event->time_msec, event->orientation,
                               event->delta, event->delta_discrete,
                               event->source, event->relative_direction);
#endif
}

inline int expectedSeatProtocolVersion() {
#if WLR_VERSION_MINOR < 20
  return 8;
#else
  return 9;
#endif
}

} // namespace LuDash::WlrootsCompat
