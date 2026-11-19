#include "compositor/input/PointerComponent.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include <linux/input-event-codes.h>

namespace LunaDash {

bool WaylandCompositor::Impl::PointerComponent::start() {
  auto *cursor = impl->cursor;
  auto *seat = impl->seat;
  listen<&PointerComponent::onMotion>(&cursor->events.motion, motion);
  listen<&PointerComponent::onMotionAbsolute>(&cursor->events.motion_absolute,
                                              motionAbsolute);
  listen<&PointerComponent::onButton>(&cursor->events.button, button);
  listen<&PointerComponent::onAxis>(&cursor->events.axis, axis);
  listen<&PointerComponent::onFrame>(&cursor->events.frame, frame);
  listen<&PointerComponent::onRequestCursor>(&seat->events.request_set_cursor,
                                             requestCursor);
  listen<&PointerComponent::onFocusChange>(
      &seat->pointer_state.events.focus_change, focusChange);
  impl->cursorShapeManager =
      wlr_cursor_shape_manager_v1_create(impl->display, 1);
  if (!impl->cursorShapeManager)
    return impl->fail("wlroots could not create the cursor-shape global.");
  listen<&PointerComponent::onRequestCursorShape>(
      &impl->cursorShapeManager->events.request_set_shape, requestCursorShape);
  return true;
}

void WaylandCompositor::Impl::PointerComponent::onMotion(
    wlr_pointer_motion_event *event) {
  if (!event)
    return;
  if (impl->idleNotify)
    impl->idleNotify->activity();
  wlr_cursor_move(impl->cursor, &event->pointer->base, event->delta_x,
                  event->delta_y);
  impl->processPointerMotion(event->time_msec);
}

void WaylandCompositor::Impl::PointerComponent::onMotionAbsolute(
    wlr_pointer_motion_absolute_event *event) {
  if (!event)
    return;
  if (impl->idleNotify)
    impl->idleNotify->activity();
  wlr_cursor_warp_absolute(impl->cursor, &event->pointer->base, event->x,
                           event->y);
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
    if (!pressed &&
        impl->orbitPointerConsumed) {
      impl->orbitPointerConsumed = false;
      return;
    }
  }
  if (desktop->overviewPointerButton(event->button, pressed))
    return;
  if (impl->pointerWindow) {
    if (!pressed &&
        event->button == impl->pointerButton) {
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
  if (!grabbed && pressed && impl->beginWindowPointer(event->button))
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
    // sx, sy are surface coordinates; a zoomed window draws them scaled.
    const auto *owner = impl->clientForSurface(surface);
    impl->pressedSurfaceScale = owner ? clientContentScale(*owner) : 1.0;
    impl->pressedSurfaceOrigin =
        PointF(impl->cursor->x - sx * impl->pressedSurfaceScale,
               impl->cursor->y - sy * impl->pressedSurfaceScale);
  }
  wlr_seat_pointer_notify_button(seat, event->time_msec, event->button,
                                 event->state);
  if (!pressed && seat->pointer_state.button_count == 0 && impl->pressedSurface) {
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

  if (event && !event->new_surface)
    impl->schedulePointerRebase();
}

} // namespace LunaDash
