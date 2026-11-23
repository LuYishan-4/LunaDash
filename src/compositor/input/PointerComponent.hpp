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
#if LUDASH_WLR_HAS_POINTER_CONSTRAINTS
  void onNewConstraint(wlr_pointer_constraint_v1 *constraint);
  void onConstraintDestroy();
  void updateConstraint();
  bool confineMotion(double &dx, double &dy);
#endif
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
#if LUDASH_WLR_HAS_POINTER_CONSTRAINTS
  Slot newConstraint;
  Slot activeConstraintDestroy;
  wlr_pointer_constraints_v1 *constraints_ = nullptr;
  wlr_pointer_constraint_v1 *activeConstraint_ = nullptr;
#endif
#if LUDASH_WLR_HAS_RELATIVE_POINTER
  wlr_relative_pointer_manager_v1 *relativePointerManager_ = nullptr;
#endif
};

} // namespace LunaDash
