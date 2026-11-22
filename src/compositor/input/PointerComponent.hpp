#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {

// The seat's pointer: motion, buttons and scrolling from the cursor, and the
// cursor images clients ask for (surfaces and cursor-shape names).
class WaylandCompositor::Impl::PointerComponent final
    : public Component,
      public LunaClint<PointerComponent> {
public:
  using Component::Component;
  bool start() override;

private:
  void onMotion(wlr_pointer_motion_event *event);
  void onMotionAbsolute(wlr_pointer_motion_absolute_event *event);
  void onButton(wlr_pointer_button_event *event);
  void onAxis(wlr_pointer_axis_event *event);
  void onFrame();
  void onRequestCursor(wlr_seat_pointer_request_set_cursor_event *event);
  void onRequestCursorShape(
      wlr_cursor_shape_manager_v1_request_set_shape_event *event);
  void onFocusChange(wlr_seat_pointer_focus_change_event *event);
  // A left press outside every shell surface closes the shell's popups.
  void dismissShellPopups();

  Slot motion;
  Slot motionAbsolute;
  Slot button;
  Slot axis;
  Slot frame;
  Slot requestCursor;
  Slot requestCursorShape;
  Slot focusChange;
};

} // namespace LunaDash
