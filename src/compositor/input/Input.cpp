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

void refreshKeyboardLeds(wlr_keyboard *keyboard) {
  if (!keyboard || !keyboard->xkb_state)
    return;
  uint32_t leds = 0;
  for (size_t index = 0; index < WLR_LED_COUNT; ++index) {
    if (keyboard->led_indexes[index] != XKB_LED_INVALID &&
        xkb_state_led_index_is_active(keyboard->xkb_state,
                                      keyboard->led_indexes[index]))
      leds |= 1u << index;
  }
  wlr_keyboard_led_update(keyboard, leds);
}

bool copyKeyboardState(wlr_keyboard *target, const wlr_keyboard *source) {
  if (!target || !source || !target->xkb_state || !source->xkb_state ||
      !target->keymap || !source->keymap ||
      !wlr_keyboard_keymaps_match(target->keymap, source->keymap))
    return false;

  xkb_state_update_mask(target->xkb_state, source->modifiers.depressed,
                        source->modifiers.latched, source->modifiers.locked,
                        0, 0, source->modifiers.group);
  target->modifiers.depressed =
      xkb_state_serialize_mods(target->xkb_state, XKB_STATE_MODS_DEPRESSED);
  target->modifiers.latched =
      xkb_state_serialize_mods(target->xkb_state, XKB_STATE_MODS_LATCHED);
  target->modifiers.locked =
      xkb_state_serialize_mods(target->xkb_state, XKB_STATE_MODS_LOCKED);
  target->modifiers.group =
      xkb_state_serialize_layout(target->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
  refreshKeyboardLeds(target);
  return true;
}

bool copyKeyboardLocks(wlr_keyboard *target, const wlr_keyboard *source) {
  if (!target || !source || !target->xkb_state || !source->xkb_state ||
      !target->keymap || !source->keymap)
    return false;

  xkb_mod_mask_t locked = target->modifiers.locked;
  bool mapped = false;
  for (const char *name : {XKB_MOD_NAME_CAPS, XKB_MOD_NAME_NUM}) {
    const xkb_mod_index_t sourceIndex =
        xkb_keymap_mod_get_index(source->keymap, name);
    const xkb_mod_index_t targetIndex =
        xkb_keymap_mod_get_index(target->keymap, name);
    if (sourceIndex == XKB_MOD_INVALID || targetIndex == XKB_MOD_INVALID)
      continue;
    mapped = true;
    const xkb_mod_mask_t sourceBit = xkb_mod_mask_t{1} << sourceIndex;
    const xkb_mod_mask_t targetBit = xkb_mod_mask_t{1} << targetIndex;
    if (source->modifiers.locked & sourceBit)
      locked |= targetBit;
    else
      locked &= ~targetBit;
  }

  if (!mapped) {
    if (!wlr_keyboard_keymaps_match(target->keymap, source->keymap))
      return false;
    locked = source->modifiers.locked;
  }

  xkb_state_update_mask(target->xkb_state, target->modifiers.depressed,
                        target->modifiers.latched, locked, 0, 0,
                        target->modifiers.group);
  target->modifiers.depressed =
      xkb_state_serialize_mods(target->xkb_state, XKB_STATE_MODS_DEPRESSED);
  target->modifiers.latched =
      xkb_state_serialize_mods(target->xkb_state, XKB_STATE_MODS_LATCHED);
  target->modifiers.locked =
      xkb_state_serialize_mods(target->xkb_state, XKB_STATE_MODS_LOCKED);
  target->modifiers.group =
      xkb_state_serialize_layout(target->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
  refreshKeyboardLeds(target);
  return true;
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

bool WaylandCompositor::Impl::inputMethodVirtualKeyboard(
    const KeyboardState *state) const {
  if (!state || !state->virtualKeyboard || !state->ownerClient ||
      !inputMethod || !inputMethod->method || !inputMethod->method->resource)
    return false;
  return state->ownerClient ==
         wl_resource_get_client(inputMethod->method->resource);
}

void WaylandCompositor::Impl::focusSurface(wlr_surface *surface) {
  if (!surface)
    return;
  // Keep a non-IME virtual keyboard as the active seat source while it owns
  // the hardware stream (GPU Screen Recorder, remote-control tools, key
  // remappers, etc.). Falling back to the physical keyboard here reintroduces
  // stale Ctrl/Caps/Num state whenever such a client changes focus.
  wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
  if (!keyboard)
    keyboard = preferredKeyboard();
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
                                          bool isVirtual,
                                          wl_client *ownerClient) {
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
  state->ownerClient = ownerClient;
  state->virtualKeyboard = isVirtual;
  state->lastLockedModifiers = keyboard->modifiers.locked;
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
  const bool inputMethodBridgeActive =
      self->activeTextInput && self->activeTextInput->text &&
      self->activeTextInput->text->focused_surface && self->inputMethod &&
      self->inputMethod->method &&
      self->inputMethod->method->keyboard_grab;
  const bool inputMethodVirtual =
      self->inputMethodVirtualKeyboard(state);

  if (state->virtualKeyboard && !inputMethodVirtual) {
    // Generic virtual keyboards such as GPU Screen Recorder may be recreated
    // repeatedly and begin with an empty locked mask. The physical keyboard is
    // the persistent lock-state owner: inherit CapsLock/NumLock before wlroots
    // processes this virtual key, then the modifiers callback writes the
    // resulting state back after the key has toggled it.
    if (auto *physical = self->preferredKeyboard();
        physical && physical != keyboard)
      copyKeyboardLocks(keyboard, physical);
  }

  if (inputMethodVirtual) {
    // The IME's own virtual keyboard is the return path from its hardware
    // keyboard grab. Keep the seat on the physical keymap and preserve the
    // physical lock/modifier state which Fcitx expects.
    self->restorePreferredKeyboard();
    if (inputMethodBridgeActive) {
      if (auto *physical = self->preferredKeyboard();
          physical && physical != keyboard)
        wlr_seat_keyboard_notify_modifiers(self->seat,
                                           &physical->modifiers);
    }
  } else {
    // Generic virtual keyboards are real input sources. GPU Screen Recorder
    // grabs the evdev keyboard and re-emits it through virtual-keyboard-v1;
    // forcing the stale physical keyboard here loses Ctrl and lock state.
    wlr_seat_set_keyboard(self->seat, keyboard);
  }

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

  // F12 is a fixed compositor shortcut for true fullscreen. Keep it outside
  // the configurable shortcut table, whose policy intentionally requires
  // Meta or Alt combinations.
  if (!state->virtualKeyboard &&
      event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
      !self->q->shortcutCapture_) {
    const uint32_t rawModifiers = wlr_keyboard_get_modifiers(keyboard);
    for (int i = 0; i < count; ++i) {
      if (symbols[i] == XKB_KEY_F12 &&
          !(rawModifiers & (WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT |
                            WLR_MODIFIER_SHIFT | WLR_MODIFIER_LOGO))) {
        self->q->handleShortcut("toggleFullscreen");
        state->consumedKeys.insert(event->keycode);
        return;
      }
    }
  }

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
      } else if (tab && (modifiers & WLR_MODIFIER_LOGO) &&
                 !(modifiers & (WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT))) {
        self->q->beginWindowSwitch(modifiers & WLR_MODIFIER_SHIFT ? -1 : 1, true);
        handled = true;
      } else if (self->q->windowSwitcher_->active()) {
        if (symbol == XKB_KEY_Escape)
          self->q->finishWindowSwitch(false);
        else if (symbol == XKB_KEY_Return)
          self->q->finishWindowSwitch(true);
        else if (symbol == XKB_KEY_Left || symbol == XKB_KEY_Right)
          self->q->windowSwitcher_->step(symbol == XKB_KEY_Left ? -1 : 1);
        else if (symbol == XKB_KEY_Up || symbol == XKB_KEY_Down)
          self->q->windowSwitcher_->step(
              (symbol == XKB_KEY_Up ? -1 : 1) *
              (self->q->windowSwitcher_->scope() == WindowSwitcher::Scope::Workspaces ? 5 : 1));
        handled = symbol != XKB_KEY_Alt_L && symbol != XKB_KEY_Alt_R &&
                  symbol != XKB_KEY_Shift_L && symbol != XKB_KEY_Shift_R &&
                  !metaKey;
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

  const bool inputMethodOwnsKeyboard =
      inputMethodBridgeActive && !inputMethodVirtual;
  if (inputMethodOwnsKeyboard) {
    auto *grab = self->inputMethod->method->keyboard_grab;
    if (grab->keyboard != keyboard)
      wlr_input_method_keyboard_grab_v2_set_keyboard(grab, keyboard);
    wlr_input_method_keyboard_grab_v2_send_key(
        grab, event->time_msec, event->keycode, event->state);
    return;
  }

  wlr_seat_keyboard_notify_key(self->seat, event->time_msec, event->keycode,
                               event->state);
  if (inputMethodVirtual)
    self->restorePreferredKeyboard();
}

void WaylandCompositor::Impl::handleKeyboardModifiers(wl_listener *listener,
                                                      void *) {
  auto *state = listenerOwner<KeyboardState>(listener);
  if (!state || !state->keyboard)
    return;
  auto *self = state->impl;

  const bool inputMethodBridgeActive =
      self->activeTextInput && self->activeTextInput->text &&
      self->activeTextInput->text->focused_surface && self->inputMethod &&
      self->inputMethod->method &&
      self->inputMethod->method->keyboard_grab;
  const bool inputMethodVirtual =
      self->inputMethodVirtualKeyboard(state);

  // Only the input method's own virtual keyboard is a return path. Other
  // virtual keyboards (notably gsr-ui virtual keyboard) replace a grabbed
  // hardware stream and must keep their own depressed/latched/locked state.
  if (inputMethodVirtual) {
    self->restorePreferredKeyboard();
    if (inputMethodBridgeActive) {
      if (auto *physical = self->preferredKeyboard();
          physical && physical != state->keyboard) {
        wlr_seat_keyboard_notify_modifiers(self->seat,
                                           &physical->modifiers);
        return;
      }
    }
    wlr_seat_keyboard_notify_modifiers(self->seat,
                                       &state->keyboard->modifiers);
    return;
  }

  wlr_seat_set_keyboard(self->seat, state->keyboard);

  // wlroots updates xkb_state before emitting this modifiers signal. Track the
  // raw locked mask separately so CapsLock/NumLock transitions remain visible
  // even while an input-method-v2 keyboard grab is active.
  const uint32_t locked = state->keyboard->modifiers.locked;
  const bool lockedChanged = locked != state->lastLockedModifiers;
  state->lastLockedModifiers = locked;

  if (state->virtualKeyboard && !inputMethodVirtual) {
    if (auto *physical = self->preferredKeyboard();
        physical && physical != state->keyboard)
      copyKeyboardLocks(physical, state->keyboard);
  }

  const bool inputMethodOwnsKeyboard =
      inputMethodBridgeActive && !inputMethodVirtual;
  if (inputMethodOwnsKeyboard) {
    auto modifiers = state->keyboard->modifiers;
    auto *grab = self->inputMethod->method->keyboard_grab;
    if (grab->keyboard != state->keyboard)
      wlr_input_method_keyboard_grab_v2_set_keyboard(grab, state->keyboard);
    wlr_input_method_keyboard_grab_v2_send_modifiers(
        grab, &modifiers);
    if (lockedChanged) {
      // Lock state belongs to the physical keyboard and must also reach the
      // focused wl_keyboard. Ordinary Shift/Ctrl/Alt remain owned by the IME.
      wlr_seat_keyboard_send_modifiers(self->seat, &modifiers);
      ++self->q->modifierResends_;
    }
  } else {
    wlr_seat_keyboard_notify_modifiers(self->seat, &state->keyboard->modifiers);
  }
  if (!state->virtualKeyboard && self->q->windowSwitcher_->active() &&
      !(wlr_keyboard_get_modifiers(state->keyboard) &
        (self->q->windowSwitcher_->scope() == WindowSwitcher::Scope::Workspaces
             ? WLR_MODIFIER_LOGO : WLR_MODIFIER_ALT)))
    self->q->finishWindowSwitch(true);
  if (inputMethodVirtual)
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

  // A hotkey/remapping application may own the physical evdev stream for its
  // whole lifetime. When its generic virtual keyboard disappears, carry its
  // final state back to the matching physical keymap before selecting it
  // again. This clears a Ctrl released through the virtual stream and keeps
  // CapsLock/NumLock in sync across recorder restarts.
  auto *current = self->seat ? wlr_seat_get_keyboard(self->seat) : nullptr;
  auto *physical = state->virtualKeyboard ? self->preferredKeyboard() : nullptr;
  if (state->virtualKeyboard && current == state->keyboard && physical &&
      physical != state->keyboard) {
    if (!copyKeyboardState(physical, state->keyboard))
      copyKeyboardLocks(physical, state->keyboard);
  }

  detachListener(state->key);
  detachListener(state->modifiers);
  detachListener(state->destroy);
  self->keyboards.removeAll(state);
  delete state;

  if (physical) {
    wlr_seat_set_keyboard(self->seat, physical);
    wlr_seat_keyboard_notify_modifiers(self->seat, &physical->modifiers);
    ++self->q->modifierResends_;
  } else {
    self->restorePreferredKeyboard();
  }
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
  if (event->button == BTN_FORWARD) {
    auto *keyboard = self->preferredKeyboard();
    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED && keyboard &&
        (wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_LOGO)) {
      self->orbitPointerConsumed = true;
      self->q->handleShortcut("launchOrbit");
      return;
    }
    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED && self->orbitPointerConsumed) {
      self->orbitPointerConsumed = false;
      return;
    }
  }
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
  if (!grabbed && event->button == BTN_LEFT &&
      event->state == static_cast<decltype(event->state)>(WL_POINTER_BUTTON_STATE_PRESSED)) {
    double sx = 0, sy = 0;
    auto *surface = self->surfaceAt(self->cursor->x, self->cursor->y, &sx, &sy);
    auto *root = surface ? wlr_surface_get_root_surface(surface) : nullptr;
    bool shellSurface = false;
    for (int depth = 0; root && depth < 16 && !shellSurface; ++depth) {
      for (const auto *layer : self->layers) {
        if (layer->surface && layer->surface->surface == root &&
            layer->surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
          shellSurface = true;
          break;
        }
      }
      if (shellSurface)
        break;
      wlr_surface *parent = nullptr;
      for (const auto *popup : self->xdgPopups)
        if (popup->popup && popup->popup->base->surface == root) {
          parent = popup->popup->parent;
          break;
        }
      if (!parent || parent == root)
        break;
      root = wlr_surface_get_root_surface(parent);
    }
    if (!shellSurface)
      self->q->windowSwitcher_->dismissPopups();
  }
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
  auto *self = state->impl;
  if (auto *keyboard = self->preferredKeyboard()) {
    wlr_input_method_keyboard_grab_v2_set_keyboard(grab, keyboard);
    auto modifiers = keyboard->modifiers;
    wlr_input_method_keyboard_grab_v2_send_modifiers(grab, &modifiers);
    wlr_seat_set_keyboard(self->seat, keyboard);
    wlr_seat_keyboard_send_modifiers(self->seat, &modifiers);
    ++self->q->modifierResends_;
  }
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
  if (auto *keyboard = self->preferredKeyboard()) {
    wlr_seat_set_keyboard(self->seat, keyboard);
    wlr_seat_keyboard_notify_modifiers(self->seat, &keyboard->modifiers);
    ++self->q->modifierResends_;
  }
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
    self->addKeyboard(&keyboard->keyboard, true,
                      keyboard->resource
                          ? wl_resource_get_client(keyboard->resource)
                          : nullptr);
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
