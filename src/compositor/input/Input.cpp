#include "compositor/client/ClientWindow.hpp"
#include "compositor/input/Keyboard.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include <QDateTime>
#include <QTimer>
#include <algorithm>
#include <linux/input-event-codes.h>

namespace LunaDash {
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

namespace {
uint32_t shortcutModifiers(uint32_t wlrModifiers) {
  uint32_t result = 0;
  if (wlrModifiers & WLR_MODIFIER_LOGO)
    result |= ShortcutMeta;
  if (wlrModifiers & WLR_MODIFIER_CTRL)
    result |= ShortcutControl;
  if (wlrModifiers & WLR_MODIFIER_ALT)
    result |= ShortcutAlt;
  if (wlrModifiers & WLR_MODIFIER_SHIFT)
    result |= ShortcutShift;
  return result;
}

bool keypadSymbol(xkb_keysym_t symbol) {
  return symbol >= XKB_KEY_KP_Space && symbol <= XKB_KEY_KP_Equal;
}

} // namespace
void WaylandCompositor::Impl::processPointerMotion(uint32_t time) {
  if (updateWindowPointer())
    return;
  double sx = 0;
  double sy = 0;
  wlr_surface *surface = surfaceAt(cursor->x, cursor->y, &sx, &sy);
  if (surface) {
    wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(seat, time, sx, sy);
  } else {
    wlr_seat_pointer_notify_clear_focus(seat);
    wlr_cursor_set_xcursor(cursor, cursorManager, "default");
  }
}

void WaylandCompositor::Impl::restorePreferredKeyboard() {
  if (auto *keyboard = preferredKeyboard())
    wlr_seat_set_keyboard(seat, keyboard);
}

void WaylandCompositor::Impl::focusSurface(wlr_surface *surface) {
  if (!surface)
    return;
  wlr_keyboard *keyboard = preferredKeyboard();
  if (!keyboard)
    return;
  wlr_seat_set_keyboard(seat, keyboard);
  QList<uint32_t> pressed;
  QSet<uint32_t> consumed;
  for (const auto *state : keyboards)
    if (state->keyboard == keyboard)
      consumed = state->consumedKeys;
  for (size_t i = 0; i < keyboard->num_keycodes; ++i)
    if (!consumed.contains(keyboard->keycodes[i]))
      pressed.append(keyboard->keycodes[i]);
  wlr_seat_keyboard_notify_enter(seat, surface, pressed.data(), pressed.size(),
                                 &keyboard->modifiers);
}

void WaylandCompositor::Impl::updateSeatCapabilities() {
  uint32_t caps = 0;
  if (pointerDevices > 0)
    caps |= WL_SEAT_CAPABILITY_POINTER;
  if (!keyboards.isEmpty())
    caps |= WL_SEAT_CAPABILITY_KEYBOARD;
  if (!caps)
    caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
  wlr_seat_set_capabilities(seat, caps);
}

void WaylandCompositor::Impl::addKeyboard(wlr_keyboard *keyboard,
                                          bool isVirtual) {
  if (!keyboard)
    return;
  if (!isVirtual) {
    QString error;
    if (!applyKeyboardPreferences(keyboard, desktopPreferences(), &error))
      qWarning().noquote() << "LunaDash keyboard:" << error;
  }

  auto *state = new KeyboardState;
  state->impl = this;
  state->keyboard = keyboard;
  state->virtualKeyboard = isVirtual;
  attachListener(&keyboard->events.key, state->key, state, handleKeyboardKey);
  attachListener(&keyboard->events.modifiers, state->modifiers, state,
                 handleKeyboardModifiers);
  attachListener(&keyboard->base.events.destroy, state->destroy, state,
                 handleKeyboardDestroy);
  keyboards.append(state);
  if (!isVirtual)
    wlr_seat_set_keyboard(seat, keyboard);
  else
    restorePreferredKeyboard();
  updateSeatCapabilities();
}

void WaylandCompositor::Impl::applyKeyboardConfig() {
  for (auto *state : keyboards) {
    if (!state || state->virtualKeyboard)
      continue;
    QString error;
    if (!applyKeyboardPreferences(state->keyboard, desktopPreferences(),
                                  &error))
      qWarning().noquote() << "LunaDash keyboard:" << error;
  }
}

void WaylandCompositor::Impl::syncTextInputToMethod(TextInputState *state,
                                                    bool activate) {
  if (!state || !state->text || !inputMethod || !inputMethod->method)
    return;
  auto *text = state->text;
  auto *method = inputMethod->method;

  if (activate) {
    activeTextInput = state;
    wlr_input_method_v2_send_activate(method);
  }

  const auto &current = text->current;
  if (current.features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT) {
    wlr_input_method_v2_send_surrounding_text(
        method, current.surrounding.text ? current.surrounding.text : "",
        current.surrounding.cursor, current.surrounding.anchor);
  }
  if (current.features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE) {
    wlr_input_method_v2_send_content_type(method, current.content_type.hint,
                                          current.content_type.purpose);
  }
  wlr_input_method_v2_send_text_change_cause(method, current.text_change_cause);
  wlr_input_method_v2_send_done(method);
}

void WaylandCompositor::Impl::deactivateTextInput(TextInputState *state) {
  if (!state || activeTextInput != state)
    return;
  if (inputMethod && inputMethod->method) {
    wlr_input_method_v2_send_deactivate(inputMethod->method);
    wlr_input_method_v2_send_done(inputMethod->method);
  }
  activeTextInput = nullptr;
}

void WaylandCompositor::Impl::updateTextInputFocus(wlr_surface *surface) {
  for (auto *state : textInputs) {
    if (!state || !state->text)
      continue;
    auto *text = state->text;
    if (text->focused_surface && text->focused_surface != surface) {
      deactivateTextInput(state);
      if (text->focused_surface->resource)
        wlr_text_input_v3_send_leave(text);
    }

    if (!surface || !surface->resource || !text->resource)
      continue;
    if (wl_resource_get_client(surface->resource) ==
        wl_resource_get_client(text->resource)) {
      if (text->focused_surface != surface)
        wlr_text_input_v3_send_enter(text, surface);
      if (text->current_enabled || text->pending_enabled)
        syncTextInputToMethod(state, true);
    }
  }
}

void WaylandCompositor::Impl::positionInputPopup(PopupState *popup) {
  if (!popup || !popup->sceneTree || !activeTextInput || !activeTextInput->text)
    return;
  wlr_surface *focus = activeTextInput->text->focused_surface;
  ClientWindow *client = clientForSurface(focus);
  if (!client)
    return;
  const auto &rect = activeTextInput->text->current.cursor_rectangle;
  wlr_scene_node_set_position(&popup->sceneTree->node,
                              client->geometry.x() + rect.x,
                              client->geometry.y() + rect.y + rect.height);
}

void WaylandCompositor::Impl::handleNewInput(wl_listener *listener,
                                             void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *device = static_cast<wlr_input_device *>(data);
  if (!self || !device)
    return;
  switch (device->type) {
  case WLR_INPUT_DEVICE_KEYBOARD:
    self->addKeyboard(wlr_keyboard_from_input_device(device), false);
    break;
  case WLR_INPUT_DEVICE_POINTER:
    ++self->pointerDevices;
    wlr_cursor_attach_input_device(self->cursor, device);
    self->updateSeatCapabilities();
    break;
  default:
    break;
  }
}

void WaylandCompositor::Impl::handleKeyboardKey(wl_listener *listener,
                                                void *data) {
  auto *state = listenerOwner<KeyboardState>(listener);
  auto *event = static_cast<wlr_keyboard_key_event *>(data);
  if (!state || !event || !state->keyboard)
    return;
  auto *self = state->impl;
  auto *keyboard = state->keyboard;
  wlr_seat_set_keyboard(self->seat, keyboard);

  const uint32_t keycode = event->keycode + 8;
  const xkb_keysym_t *symbols = nullptr;
  const int count =
      keyboard->xkb_state
          ? xkb_state_key_get_syms(keyboard->xkb_state, keycode, &symbols)
          : 0;

  bool metaKey = false;
  for (int i = 0; i < count; ++i)
    metaKey = metaKey || symbols[i] == XKB_KEY_Meta_L ||
              symbols[i] == XKB_KEY_Meta_R ||
              symbols[i] == XKB_KEY_Super_L ||
              symbols[i] == XKB_KEY_Super_R;

  // Reserve a quick press-and-release of Meta for the shell launcher without
  // breaking the existing Meta+key compositor shortcuts. The modifier event is
  // still delivered through wl_keyboard.modifiers, so an unbound Meta combo
  // can continue to reach a client even though the bare modifier key event is
  // kept by the compositor.
  if (!state->virtualKeyboard && !self->q->shortcutCapture_) {
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED && metaKey) {
      const auto modifiers = wlr_keyboard_get_modifiers(keyboard);
      if (!(modifiers & (WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT |
                         WLR_MODIFIER_SHIFT))) {
        state->metaTapPending = true;
        state->metaTapKeycode = static_cast<int>(event->keycode);
        return;
      }
    } else if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
               state->metaTapPending) {
      state->metaTapPending = false;
    }

    if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
        state->metaTapKeycode == static_cast<int>(event->keycode)) {
      const bool openLauncher = state->metaTapPending;
      state->metaTapPending = false;
      state->metaTapKeycode = -1;
      if (openLauncher)
        self->q->handleShortcut("launchLauncher");
      return;
    }
  }

  if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
      state->consumedKeys.remove(event->keycode))
    return;
  const bool shortcutPress = !state->virtualKeyboard &&
                             event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
                             !self->q->shortcutCapture_;
  if (shortcutPress)
    state->consumedKeys.insert(event->keycode);
  bool handled = false;
  if (!state->virtualKeyboard &&
      event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
      !self->q->shortcutCapture_) {
    const auto modifiers = wlr_keyboard_get_modifiers(keyboard);
    for (int i = 0; i < count; ++i) {
      const auto symbol = symbols[i];
      const bool tab = symbol == XKB_KEY_Tab || symbol == XKB_KEY_ISO_Left_Tab;
      if (tab && (modifiers & WLR_MODIFIER_ALT) &&
          !(modifiers & (WLR_MODIFIER_CTRL | WLR_MODIFIER_LOGO))) {
        self->q->beginWindowSwitch(modifiers & WLR_MODIFIER_SHIFT ? -1 : 1);
        handled = true;
      } else if (self->q->windowSwitcher_->active()) {
        if (symbol == XKB_KEY_Escape)
          self->q->finishWindowSwitch(false);
        else if (symbol == XKB_KEY_Return)
          self->q->finishWindowSwitch(true);
        else if (symbol == XKB_KEY_Left || symbol == XKB_KEY_Right)
          self->q->windowSwitcher_->step(symbol == XKB_KEY_Left ? -1 : 1);
        else if (symbol == XKB_KEY_Up || symbol == XKB_KEY_Down)
          self->q->windowSwitcher_->step(symbol == XKB_KEY_Up ? -5 : 5);
        handled = symbol != XKB_KEY_Alt_L && symbol != XKB_KEY_Alt_R &&
                  symbol != XKB_KEY_Shift_L && symbol != XKB_KEY_Shift_R;
      }
    }
  }
  if (!handled && !state->virtualKeyboard &&
      event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
      !self->q->shortcutCapture_) {
    const uint32_t modifiers =
        shortcutModifiers(wlr_keyboard_get_modifiers(keyboard));
    for (int i = 0; i < count && !handled; ++i) {
      const QString action =
          self->q->shortcutSettings_->actionFor(symbols[i], modifiers);
      if (!action.isEmpty()) {
        self->q->handleShortcut(action);
        handled = true;
      }
    }
  }

  if (!handled && !state->virtualKeyboard)
    for (int i = 0; i < count; ++i)
      if (keypadSymbol(symbols[i])) {
        ++self->q->keypadKeyForwards_;
        break;
      }

  if (handled) {
    state->consumedKeys.insert(event->keycode);
    return;
  }

  if (shortcutPress)
    state->consumedKeys.remove(event->keycode);

  if (self->inputMethod && self->inputMethod->method &&
      self->inputMethod->method->keyboard_grab && !state->virtualKeyboard) {
    wlr_input_method_keyboard_grab_v2_send_key(
        self->inputMethod->method->keyboard_grab, event->time_msec,
        event->keycode, event->state);
    return;
  }

  wlr_seat_keyboard_notify_key(self->seat, event->time_msec, event->keycode,
                               event->state);
  if (state->virtualKeyboard)
    self->restorePreferredKeyboard();
}

void WaylandCompositor::Impl::handleKeyboardModifiers(wl_listener *listener,
                                                      void *) {
  auto *state = listenerOwner<KeyboardState>(listener);
  if (!state || !state->keyboard)
    return;
  auto *self = state->impl;
  wlr_seat_set_keyboard(self->seat, state->keyboard);
  if (self->inputMethod && self->inputMethod->method &&
      self->inputMethod->method->keyboard_grab && !state->virtualKeyboard) {
    auto modifiers = state->keyboard->modifiers;
    wlr_input_method_keyboard_grab_v2_send_modifiers(
        self->inputMethod->method->keyboard_grab, &modifiers);
  } else {
    wlr_seat_keyboard_notify_modifiers(self->seat, &state->keyboard->modifiers);
  }
  if (!state->virtualKeyboard && self->q->windowSwitcher_->active() &&
      !(wlr_keyboard_get_modifiers(state->keyboard) & WLR_MODIFIER_ALT))
    self->q->finishWindowSwitch(true);
  if (state->virtualKeyboard)
    self->restorePreferredKeyboard();
}

void WaylandCompositor::Impl::handleKeyboardDestroy(wl_listener *listener,
                                                    void *) {
  auto *state = listenerOwner<KeyboardState>(listener);
  if (!state)
    return;
  auto *self = state->impl;
  if (!state->virtualKeyboard)
    self->q->finishWindowSwitch(false);
  detachListener(state->key);
  detachListener(state->modifiers);
  detachListener(state->destroy);
  self->keyboards.removeAll(state);
  delete state;
  self->restorePreferredKeyboard();
  self->updateSeatCapabilities();
}

void WaylandCompositor::Impl::handleCursorMotion(wl_listener *listener,
                                                 void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *event = static_cast<wlr_pointer_motion_event *>(data);
  if (!self || !event)
    return;
  wlr_cursor_move(self->cursor, &event->pointer->base, event->delta_x,
                  event->delta_y);
  self->processPointerMotion(event->time_msec);
}

void WaylandCompositor::Impl::handleCursorMotionAbsolute(wl_listener *listener,
                                                         void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *event = static_cast<wlr_pointer_motion_absolute_event *>(data);
  if (!self || !event)
    return;
  wlr_cursor_warp_absolute(self->cursor, &event->pointer->base, event->x,
                           event->y);
  self->processPointerMotion(event->time_msec);
}

void WaylandCompositor::Impl::handleCursorButton(wl_listener *listener,
                                                 void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *event = static_cast<wlr_pointer_button_event *>(data);
  if (!self || !event)
    return;
  if (self->pointerWindow) {
    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED &&
        event->button == self->pointerButton) {
      if (self->pointerClientGrab)
        wlr_seat_pointer_notify_button(self->seat, event->time_msec,
                                       event->button, event->state);
      self->finishWindowPointer(true);
    }
    return;
  }
  if (self->q->windowSwitcher_->active()) {
    double sx = 0, sy = 0;
    if (self->clientForSurface(
            self->surfaceAt(self->cursor->x, self->cursor->y, &sx, &sy)))
      return;
  }
  const bool grabbed = wlr_seat_pointer_has_grab(self->seat);
  if (!grabbed && event->state == WL_POINTER_BUTTON_STATE_PRESSED &&
      self->beginWindowPointer(event->button))
    return;
  if (event->state == WL_POINTER_BUTTON_STATE_PRESSED && !grabbed) {
    double sx = 0;
    double sy = 0;
    wlr_surface *surface =
        self->surfaceAt(self->cursor->x, self->cursor->y, &sx, &sy);
    // Deliver focus before the press which may open a grabbed popup. Do not
    // send a redundant toplevel configure after every context-menu click.
    if (auto *client = self->clientForSurface(surface))
      self->q->focus(client);
  }
  wlr_seat_pointer_notify_button(self->seat, event->time_msec, event->button,
                                 event->state);
}

void WaylandCompositor::Impl::handleCursorAxis(wl_listener *listener,
                                               void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *event = static_cast<wlr_pointer_axis_event *>(data);
  if (self && event && self->q->windowSwitcher_->active()) {
    if (event->delta != 0)
      self->q->windowSwitcher_->step(event->delta < 0 ? -1 : 1);
    return;
  }
  if (self && event)
    WlrootsCompat::notifyPointerAxis(self->seat, event);
}

void WaylandCompositor::Impl::handleCursorFrame(wl_listener *listener, void *) {
  auto *self = listenerOwner<Impl>(listener);
  if (self)
    wlr_seat_pointer_notify_frame(self->seat);
}

void WaylandCompositor::Impl::handleRequestCursor(wl_listener *listener,
                                                  void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *event = static_cast<wlr_seat_pointer_request_set_cursor_event *>(data);
  if (!self || !event)
    return;
  if (self->seat->pointer_state.focused_client == event->seat_client)
    wlr_cursor_set_surface(self->cursor, event->surface, event->hotspot_x,
                           event->hotspot_y);
}

void WaylandCompositor::Impl::handleRequestSelection(wl_listener *listener,
                                                     void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *event = static_cast<wlr_seat_request_set_selection_event *>(data);
  if (self && event)
    wlr_seat_set_selection(self->seat, event->source, event->serial);
}

void WaylandCompositor::Impl::handleKeyboardFocusChange(wl_listener *listener,
                                                        void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *event = static_cast<wlr_seat_keyboard_focus_change_event *>(data);
  if (self && event)
    self->updateTextInputFocus(event->new_surface);
}

void WaylandCompositor::Impl::handleNewInputMethod(wl_listener *listener,
                                                   void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *method = static_cast<wlr_input_method_v2 *>(data);
  if (!self || !method)
    return;
  if (self->inputMethod) {
    wlr_input_method_v2_send_unavailable(method);
    return;
  }

  auto *state = new InputMethodState;
  state->impl = self;
  state->method = method;
  self->inputMethod = state;
  attachListener(&method->events.commit, state->commit, state,
                 handleInputMethodCommit);
  attachListener(&method->events.new_popup_surface, state->newPopup, state,
                 handleInputMethodPopup);
  attachListener(&method->events.grab_keyboard, state->grabKeyboard, state,
                 handleInputMethodGrab);
  attachListener(&method->events.destroy, state->destroy, state,
                 handleInputMethodDestroy);

  if (self->activeTextInput)
    self->syncTextInputToMethod(self->activeTextInput, true);
}

void WaylandCompositor::Impl::handleInputMethodCommit(wl_listener *listener,
                                                      void *) {
  auto *state = listenerOwner<InputMethodState>(listener);
  if (!state || !state->method)
    return;
  auto *self = state->impl;
  if (!self->activeTextInput || !self->activeTextInput->text)
    return;

  auto *text = self->activeTextInput->text;
  const auto &current = state->method->current;
  if (current.preedit.text)
    wlr_text_input_v3_send_preedit_string(text, current.preedit.text,
                                          current.preedit.cursor_begin,
                                          current.preedit.cursor_end);
  if (current.commit_text)
    wlr_text_input_v3_send_commit_string(text, current.commit_text);
  if (current.delete_.before_length || current.delete_.after_length)
    wlr_text_input_v3_send_delete_surrounding_text(
        text, current.delete_.before_length, current.delete_.after_length);
  wlr_text_input_v3_send_done(text);
}

void WaylandCompositor::Impl::handleInputMethodPopup(wl_listener *listener,
                                                     void *data) {
  auto *state = listenerOwner<InputMethodState>(listener);
  auto *popup = static_cast<wlr_input_popup_surface_v2 *>(data);
  if (!state || !popup)
    return;
  auto *self = state->impl;
  auto *popupState = new PopupState;
  popupState->impl = self;
  popupState->popup = popup;
  popupState->sceneTree =
      wlr_scene_subsurface_tree_create(self->overlayLayer, popup->surface);
  attachListener(&popup->events.destroy, popupState->destroy, popupState,
                 handleInputPopupDestroy);
  self->inputPopups.append(popupState);
  self->positionInputPopup(popupState);
}

void WaylandCompositor::Impl::handleInputPopupDestroy(wl_listener *listener,
                                                      void *) {
  auto *state = listenerOwner<PopupState>(listener);
  if (!state)
    return;
  auto *self = state->impl;
  detachListener(state->destroy);
  self->inputPopups.removeAll(state);
  delete state;
}

void WaylandCompositor::Impl::handleInputMethodGrab(wl_listener *listener,
                                                    void *data) {
  auto *state = listenerOwner<InputMethodState>(listener);
  auto *grab = static_cast<wlr_input_method_keyboard_grab_v2 *>(data);
  if (!state || !grab)
    return;
  if (auto *keyboard = state->impl->preferredKeyboard())
    wlr_input_method_keyboard_grab_v2_set_keyboard(grab, keyboard);
}

void WaylandCompositor::Impl::handleInputMethodDestroy(wl_listener *listener,
                                                       void *) {
  auto *state = listenerOwner<InputMethodState>(listener);
  if (!state)
    return;
  auto *self = state->impl;
  detachListener(state->commit);
  detachListener(state->newPopup);
  detachListener(state->grabKeyboard);
  detachListener(state->destroy);
  if (self->inputMethod == state)
    self->inputMethod = nullptr;
  delete state;
}

void WaylandCompositor::Impl::handleNewTextInput(wl_listener *listener,
                                                 void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *text = static_cast<wlr_text_input_v3 *>(data);
  if (!self || !text)
    return;
  auto *state = new TextInputState;
  state->impl = self;
  state->text = text;
  self->textInputs.append(state);
  attachListener(&text->events.enable, state->enable, state,
                 handleTextInputEnable);
  attachListener(&text->events.commit, state->commit, state,
                 handleTextInputCommit);
  attachListener(&text->events.disable, state->disable, state,
                 handleTextInputDisable);
  attachListener(&text->events.destroy, state->destroy, state,
                 handleTextInputDestroy);

  if (self->seat->keyboard_state.focused_surface)
    self->updateTextInputFocus(self->seat->keyboard_state.focused_surface);
}

void WaylandCompositor::Impl::handleTextInputEnable(wl_listener *listener,
                                                    void *) {
  auto *state = listenerOwner<TextInputState>(listener);
  if (!state || !state->text || !state->text->focused_surface)
    return;
  state->impl->syncTextInputToMethod(state, true);
}

void WaylandCompositor::Impl::handleTextInputCommit(wl_listener *listener,
                                                    void *) {
  auto *state = listenerOwner<TextInputState>(listener);
  if (!state || state->impl->activeTextInput != state)
    return;
  state->impl->syncTextInputToMethod(state, false);
  for (auto *popup : state->impl->inputPopups)
    state->impl->positionInputPopup(popup);
}

void WaylandCompositor::Impl::handleTextInputDisable(wl_listener *listener,
                                                     void *) {
  auto *state = listenerOwner<TextInputState>(listener);
  if (state)
    state->impl->deactivateTextInput(state);
}

void WaylandCompositor::Impl::handleTextInputDestroy(wl_listener *listener,
                                                     void *) {
  auto *state = listenerOwner<TextInputState>(listener);
  if (!state)
    return;
  auto *self = state->impl;
  self->deactivateTextInput(state);
  detachListener(state->enable);
  detachListener(state->commit);
  detachListener(state->disable);
  detachListener(state->destroy);
  self->textInputs.removeAll(state);
  delete state;
}

void WaylandCompositor::Impl::handleNewVirtualKeyboard(wl_listener *listener,
                                                       void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *keyboard = static_cast<wlr_virtual_keyboard_v1 *>(data);
  if (self && keyboard)
    self->addKeyboard(&keyboard->keyboard, true);
}
wlr_surface *WaylandCompositor::Impl::surfaceAt(double lx, double ly,
                                                double *sx, double *sy) const {
  if (!scene)
    return nullptr;
  wlr_scene_node *node = wlr_scene_node_at(&scene->tree.node, lx, ly, sx, sy);
  if (!node || node->type != WLR_SCENE_NODE_BUFFER)
    return nullptr;
  auto *buffer = wlr_scene_buffer_from_node(node);
  auto *sceneSurface = wlr_scene_surface_try_from_buffer(buffer);
  return sceneSurface ? sceneSurface->surface : nullptr;
}

wlr_keyboard *WaylandCompositor::Impl::preferredKeyboard() const {
  for (auto *state : keyboards)
    if (state && state->keyboard && !state->virtualKeyboard)
      return state->keyboard;
  for (auto *state : keyboards)
    if (state && state->keyboard)
      return state->keyboard;
  return nullptr;
}

WaylandCompositor::Impl::TextInputState *
WaylandCompositor::Impl::textState(wlr_text_input_v3 *text) const {
  for (auto *state : textInputs)
    if (state && state->text == text)
      return state;
  return nullptr;
}

} // namespace LunaDash
