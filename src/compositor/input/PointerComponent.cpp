#include "compositor/input/PointerComponent.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include <linux/input-event-codes.h>

namespace LunaDash {

bool WaylandCompositor::Impl::PointerComponent::start() {
  auto *cursor = impl->cursor;
  auto *seat = impl->seat;
  listen<&PointerComponent::onMotion, &PointerComponent::motion>(
      &cursor->events.motion);
  listen<&PointerComponent::onMotionAbsolute,
         &PointerComponent::motionAbsolute>(&cursor->events.motion_absolute);
  listen<&PointerComponent::onButton, &PointerComponent::button>(
      &cursor->events.button);
  listen<&PointerComponent::onAxis, &PointerComponent::axis>(
      &cursor->events.axis);
  listen<&PointerComponent::onFrame, &PointerComponent::frame>(
      &cursor->events.frame);
  listen<&PointerComponent::onRequestCursor, &PointerComponent::requestCursor>(
      &seat->events.request_set_cursor);
  listen<&PointerComponent::onFocusChange, &PointerComponent::focusChange>(
      &seat->pointer_state.events.focus_change);
  impl->cursorShapeManager =
      wlr_cursor_shape_manager_v1_create(impl->display, 1);
  if (!impl->cursorShapeManager)
    return impl->fail("wlroots could not create the cursor-shape global.");
  listen<&PointerComponent::onRequestCursorShape,
         &PointerComponent::requestCursorShape>(
      &impl->cursorShapeManager->events.request_set_shape);
#if LUDASH_WLR_HAS_POINTER_CONSTRAINTS
  constraints_ = wlr_pointer_constraints_v1_create(impl->display);
  if (!constraints_)
    return impl->fail(
        "wlroots could not create the pointer-constraints global.");
  listen<&PointerComponent::onNewConstraint, &PointerComponent::newConstraint>(
      &constraints_->events.new_constraint);
#endif
#if LUDASH_WLR_HAS_RELATIVE_POINTER
  relativePointerManager_ =
      wlr_relative_pointer_manager_v1_create(impl->display);
  if (!relativePointerManager_)
    return impl->fail("wlroots could not create the relative-pointer global.");
#endif
  return true;
}

void WaylandCompositor::Impl::PointerComponent::onMotion(
    wlr_pointer_motion_event *event) {
  if (!event)
    return;
  if (impl->idleNotify)
    impl->idleNotify->activity();
#if LUDASH_WLR_HAS_RELATIVE_POINTER
  if (relativePointerManager_)
    wlr_relative_pointer_manager_v1_send_relative_motion(
        relativePointerManager_, impl->seat,
        static_cast<uint64_t>(event->time_msec) * 1000, event->delta_x,
        event->delta_y, event->unaccel_dx, event->unaccel_dy);
#endif
#if LUDASH_WLR_HAS_POINTER_CONSTRAINTS
  if (activeConstraint_ &&
      activeConstraint_->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
    return;
  double dx = event->delta_x;
  double dy = event->delta_y;
  if (!confineMotion(dx, dy))
    return;
  wlr_cursor_move(impl->cursor, &event->pointer->base, dx, dy);
#else
  wlr_cursor_move(impl->cursor, &event->pointer->base, event->delta_x,
                  event->delta_y);
#endif
  impl->pointerCursor->moved();
  impl->processPointerMotion(event->time_msec);
}

void WaylandCompositor::Impl::PointerComponent::onMotionAbsolute(
    wlr_pointer_motion_absolute_event *event) {
  if (!event)
    return;
  if (impl->idleNotify)
    impl->idleNotify->activity();
#if LUDASH_WLR_HAS_POINTER_CONSTRAINTS || LUDASH_WLR_HAS_RELATIVE_POINTER
  double lx = 0;
  double ly = 0;
  wlr_cursor_absolute_to_layout_coords(impl->cursor, &event->pointer->base,
                                       event->x, event->y, &lx, &ly);
  double dx = lx - impl->cursor->x;
  double dy = ly - impl->cursor->y;
#if LUDASH_WLR_HAS_RELATIVE_POINTER
  if (relativePointerManager_)
    wlr_relative_pointer_manager_v1_send_relative_motion(
        relativePointerManager_, impl->seat,
        static_cast<uint64_t>(event->time_msec) * 1000, dx, dy, dx, dy);
#endif
#if LUDASH_WLR_HAS_POINTER_CONSTRAINTS
  if (activeConstraint_ &&
      activeConstraint_->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
    return;
  const bool confined =
      activeConstraint_ &&
      activeConstraint_->type == WLR_POINTER_CONSTRAINT_V1_CONFINED;
  if (!confineMotion(dx, dy))
    return;
  if (confined) {
    wlr_cursor_warp(impl->cursor, nullptr, impl->cursor->x + dx,
                    impl->cursor->y + dy);
  } else {
    wlr_cursor_warp_absolute(impl->cursor, &event->pointer->base, event->x,
                             event->y);
  }
#else
  wlr_cursor_warp_absolute(impl->cursor, &event->pointer->base, event->x,
                           event->y);
#endif
#else
  wlr_cursor_warp_absolute(impl->cursor, &event->pointer->base, event->x,
                           event->y);
#endif
  impl->pointerCursor->moved();
  impl->processPointerMotion(event->time_msec);
}

void WaylandCompositor::Impl::PointerComponent::onButton(
    wlr_pointer_button_event *event) {
  if (!event)
    return;
  if (impl->idleNotify)
    impl->idleNotify->activity();
  if (impl->sessionLocked()) {
    // Only to the lock surface under the pointer (processPointerMotion).
    wlr_seat_pointer_notify_button(impl->seat, event->time_msec, event->button,
                                   event->state);
    return;
  }
  auto *desktop = impl->q;
  auto *seat = impl->seat;
  const bool pressed = WlrootsCompat::buttonPressed(event);
  if (event->button == BTN_FORWARD) {
    auto *keyboard = impl->preferredKeyboard();
    if (pressed && keyboard &&
        (wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_LOGO)) {
      impl->orbitPointerConsumed = true;
      desktop->runAction("open-orbit");
      return;
    }
    if (!pressed && impl->orbitPointerConsumed) {
      impl->orbitPointerConsumed = false;
      return;
    }
  }
  if (desktop->overviewPointerButton(event->button, pressed))
    return;
  if (impl->pointerWindow) {
    if (!pressed && event->button == impl->pointerButton) {
      if (impl->pointerClientGrab)
        wlr_seat_pointer_notify_button(seat, event->time_msec, event->button,
                                       event->state);
      impl->finishWindowPointer(true);
    }
    return;
  }
  double sx = 0;
  double sy = 0;
  if (desktop->windowSwitcher_->active() &&
      impl->clientForSurface(
          impl->surfaceAt(impl->cursor->x, impl->cursor->y, &sx, &sy)))
    return;
  const bool grabbed = wlr_seat_pointer_has_grab(seat);
  if (!grabbed && event->button == BTN_LEFT && pressed)
    dismissShellPopups();
  if (!grabbed && pressed &&
      (impl->beginBorderResize(event->button) ||
       impl->beginWindowPointer(event->button)))
    return;
  if (pressed && !grabbed) {
    auto *surface = impl->surfaceAt(impl->cursor->x, impl->cursor->y, &sx, &sy);
    if (auto *client = impl->clientForSurface(surface))
      desktop->focus(client);
  }
  if (pressed && seat->pointer_state.button_count == 0) {
    // The first button down starts the implicit grab on the focused surface.
    auto *surface = impl->surfaceAt(impl->cursor->x, impl->cursor->y, &sx, &sy);
    impl->pressedSurface =
        surface && surface == seat->pointer_state.focused_surface ? surface
                                                                  : nullptr;
    impl->pressedSurfaceOrigin = PointF(impl->cursor->x - sx, impl->cursor->y - sy);
  }
  wlr_seat_pointer_notify_button(seat, event->time_msec, event->button,
                                 event->state);
  if (!pressed && seat->pointer_state.button_count == 0 &&
      impl->pressedSurface) {
    // The grab ends: hand the pointer to whatever is under it now.
    impl->pressedSurface = nullptr;
    impl->schedulePointerRebase();
  }
}

void WaylandCompositor::Impl::PointerComponent::dismissShellPopups() {
  double sx = 0;
  double sy = 0;
  auto *surface = impl->surfaceAt(impl->cursor->x, impl->cursor->y, &sx, &sy);
  auto *root = surface ? wlr_surface_get_root_surface(surface) : nullptr;
  // Walk from the pressed surface up through its xdg popup parents.
  for (int depth = 0; root && depth < 16; ++depth) {
    for (const auto *layer : impl->layers)
      if (layer->surface->surface == root &&
          layer->surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP)
        return;
    wlr_surface *parent = nullptr;
    for (const auto *popup : impl->xdgPopups)
      if (popup->popup->base->surface == root) {
        parent = popup->popup->parent;
        break;
      }
    if (!parent || parent == root)
      break;
    root = wlr_surface_get_root_surface(parent);
  }
  impl->q->windowSwitcher_->dismissPopups();
}

void WaylandCompositor::Impl::PointerComponent::onAxis(
    wlr_pointer_axis_event *event) {
  if (!event)
    return;
  if (impl->idleNotify)
    impl->idleNotify->activity();
  if (impl->sessionLocked()) {
    WlrootsCompat::notifyPointerAxis(impl->seat, event);
    return;
  }
  // In the overview a horizontal wheel, or Shift with the vertical one, moves
  // between the windows of a workspace.
  auto *keyboard = impl->preferredKeyboard();
  const bool shift =
      keyboard && (wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_SHIFT);
  if (impl->q->overviewAxis(event->delta,
                            !WlrootsCompat::verticalAxis(event) || shift))
    return;
  auto *switcher = impl->q->windowSwitcher_.get();
  if (switcher->active()) {
    if (event->delta != 0)
      switcher->step(event->delta < 0 ? -1 : 1);
    return;
  }
  WlrootsCompat::notifyPointerAxis(impl->seat, event);
}

void WaylandCompositor::Impl::PointerComponent::onFrame() {
  wlr_seat_pointer_notify_frame(impl->seat);
}

void WaylandCompositor::Impl::PointerComponent::onRequestCursor(
    wlr_seat_pointer_request_set_cursor_event *event) {
  if (!event || impl->pointerWindow)
    return;
  if (impl->seat->pointer_state.focused_client == event->seat_client)
    impl->setClientCursor(event->surface, event->hotspot_x, event->hotspot_y);
}

void WaylandCompositor::Impl::PointerComponent::onRequestCursorShape(
    wlr_cursor_shape_manager_v1_request_set_shape_event *event) {
  if (!event || impl->pointerWindow ||
      event->device_type != WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_POINTER)
    return;
  if (impl->seat->pointer_state.focused_client == event->seat_client)
    impl->setCursorImage(wlr_cursor_shape_v1_name(event->shape));
}

void WaylandCompositor::Impl::PointerComponent::onFocusChange(
    wlr_seat_pointer_focus_change_event *event) {
#if LUDASH_WLR_HAS_POINTER_CONSTRAINTS
  updateConstraint();
#endif
  if (event && !event->new_surface)
    impl->schedulePointerRebase();
}

#if LUDASH_WLR_HAS_POINTER_CONSTRAINTS
void WaylandCompositor::Impl::PointerComponent::onNewConstraint(
    wlr_pointer_constraint_v1 *constraint) {
  if (constraint &&
      constraint->surface == impl->seat->pointer_state.focused_surface)
    updateConstraint();
}

void WaylandCompositor::Impl::PointerComponent::onConstraintDestroy() {
  Templates::detachListener(activeConstraintDestroy);
  activeConstraint_ = nullptr;
  if (impl->pointerCursor)
    impl->pointerCursor->setHidden(false);
}

void WaylandCompositor::Impl::PointerComponent::updateConstraint() {
  auto *surface = impl->seat->pointer_state.focused_surface;
  auto *next = surface ? wlr_pointer_constraints_v1_constraint_for_surface(
                             constraints_, surface, impl->seat)
                       : nullptr;
  if (next == activeConstraint_)
    return;

  auto *previous = activeConstraint_;
  if (activeConstraintDestroy.connected)
    Templates::detachListener(activeConstraintDestroy);
  activeConstraint_ = nullptr;
  impl->pointerCursor->setHidden(false);
  if (previous)
    wlr_pointer_constraint_v1_send_deactivated(previous);

  if (!next)
    return;
  activeConstraint_ = next;
  listen<&PointerComponent::onConstraintDestroy,
         &PointerComponent::activeConstraintDestroy>(&next->events.destroy);
  wlr_pointer_constraint_v1_send_activated(next);
  if (next->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
    impl->pointerCursor->setHidden(true);
}

bool WaylandCompositor::Impl::PointerComponent::confineMotion(double &dx,
                                                              double &dy) {
  if (!activeConstraint_ ||
      activeConstraint_->type != WLR_POINTER_CONSTRAINT_V1_CONFINED)
    return true;
  double sx = 0;
  double sy = 0;
  auto *surface = impl->surfaceAt(impl->cursor->x, impl->cursor->y, &sx, &sy);
  if (surface != activeConstraint_->surface)
    return false;
  double confinedX = 0;
  double confinedY = 0;
  if (!wlr_region_confine(&activeConstraint_->region, sx, sy, sx + dx, sy + dy,
                          &confinedX, &confinedY))
    return false;
  dx = confinedX - sx;
  dy = confinedY - sy;
  return true;
}
#endif

} // namespace LunaDash
