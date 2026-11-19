#include "compositor/client/ClientWindow.hpp"
#include "compositor/input/Keyboard.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "desktop/shortcuts/KeyBindings.hpp"
#include <chrono>
#include <algorithm>
#include <linux/input-event-codes.h>
#include <unordered_set>
#include <vector>

namespace LunaDash {

namespace {
uint32_t bindingModifiers(uint32_t wlrModifiers) {
  uint32_t result = 0;
  if (wlrModifiers & WLR_MODIFIER_LOGO)
    result |= KeyMeta;
  if (wlrModifiers & WLR_MODIFIER_CTRL)
    result |= KeyControl;
  if (wlrModifiers & WLR_MODIFIER_ALT)
    result |= KeyAlt;
  if (wlrModifiers & WLR_MODIFIER_SHIFT)
    result |= KeyShift;
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
  if (sessionLocked()) {
    double sx = 0, sy = 0;
    if (auto *surface = sessionLock->surfaceAt(cursor->x, cursor->y, &sx, &sy)) {
      wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
      wlr_seat_pointer_notify_motion(seat, time, sx, sy);
    } else {
      wlr_seat_pointer_notify_clear_focus(seat);
      setCursorImage("default");
    }
    return;
  }
  if (updateWindowPointer())
    return;

  if (pressedSurface && seat->pointer_state.button_count > 0 &&
      seat->pointer_state.focused_surface == pressedSurface) {
    wlr_seat_pointer_notify_motion(
        seat, time, (cursor->x - pressedSurfaceOrigin.x()) / pressedSurfaceScale,
        (cursor->y - pressedSurfaceOrigin.y()) / pressedSurfaceScale);
    return;
  }
  pressedSurface = nullptr;
  if (q->overviewPointerMotion())
    return;
  double sx = 0;
  double sy = 0;
  wlr_surface *surface = surfaceAt(cursor->x, cursor->y, &sx, &sy);
  if (surface) {
    if (surface != seat->pointer_state.focused_surface)
      setCursorImage("default");
    wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(seat, time, sx, sy);
  } else {
    wlr_seat_pointer_notify_clear_focus(seat);
    setCursorImage("default");
  }
}

void WaylandCompositor::Impl::setCursorImage(const char *name) {
  if (!cursor || !cursorManager)
    return;
  wlr_cursor_set_xcursor(cursor, cursorManager, name);
  cursorImage = name;
}

void WaylandCompositor::Impl::setClientCursor(wlr_surface *surface,
                                              int32_t hotspotX, int32_t hotspotY) {
  if (!cursor)
    return;
  wlr_cursor_set_surface(cursor, surface, hotspotX, hotspotY);

  cursorImage = surface ? "client" : "hidden by client";
}

void WaylandCompositor::Impl::rebasePointer() {
  if (!seat || !cursor || pointerWindow)
    return;
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch());
  processPointerMotion(static_cast<uint32_t>(now.count()));
}

void WaylandCompositor::Impl::schedulePointerRebase() {
  if (pointerRebaseScheduled)
    return;
  pointerRebaseScheduled = true;
  if (!scheduleCallback(0, [this] {
        pointerRebaseScheduled = false;
        rebasePointer();
      }))
    pointerRebaseScheduled = false;
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

void WaylandCompositor::Impl::sessionLockChanged(bool locked) {
  if (locked) {
    if (pointerWindow)
      finishWindowPointer(false);
    if (q->windowSwitcher_->active())
      q->finishWindowSwitch(false);
    if (q->overviewOpen())
      q->closeOverview();
    pressedSurface = nullptr;
    wlr_seat_pointer_notify_clear_focus(seat);
  } else {
    q->synchronizeWindowFocus();
  }
  schedulePointerRebase();
}

void WaylandCompositor::Impl::focusSurface(wlr_surface *surface) {
  if (!surface)
    return;
  // Nothing but the lock screen may take the keyboard while locked.
  if (sessionLocked() && !sessionLock->isLockSurface(surface))
    return;

  wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
  if (!keyboard)
    keyboard = preferredKeyboard();
  if (!keyboard)
    return;
  wlr_seat_set_keyboard(seat, keyboard);
  std::vector<uint32_t> pressed;
  std::unordered_set<uint32_t> consumed;
  for (const auto *state : keyboards)
    if (state->keyboard == keyboard)
      consumed = state->consumedKeys;
  for (size_t i = 0; i < keyboard->num_keycodes; ++i)
    if (!consumed.contains(keyboard->keycodes[i]))
      pressed.push_back(keyboard->keycodes[i]);
  wlr_seat_keyboard_notify_enter(seat, surface, pressed.data(), pressed.size(),
                                 &keyboard->modifiers);
}

void WaylandCompositor::Impl::updateSeatCapabilities() {
  uint32_t caps = 0;
  if (pointerDevices > 0)
    caps |= WL_SEAT_CAPABILITY_POINTER;
  if (!keyboards.empty())
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
    std::string error;
    if (!applyKeyboardPreferences(keyboard, desktopPreferences(), &error))
      logWarning("LunaDash keyboard: %s", error.c_str());
  }

  auto *state = new KeyboardState(this, keyboard, isVirtual, ownerClient);
  keyboards.push_back(state);
  if (!isVirtual)
    wlr_seat_set_keyboard(seat, keyboard);
  else
    restorePreferredKeyboard();
  updateSeatCapabilities();
  state->listenForKeymap();
  if (keyboard->keymap && !seat->keyboard_state.focused_surface)
    restoreLayerFocus();
}

WaylandCompositor::Impl::KeyboardState::KeyboardState(Impl *owner,
                                                      wlr_keyboard *wlrKeyboard,
                                                      bool isVirtual,
                                                      wl_client *owningClient)
    : impl(owner), keyboard(wlrKeyboard), ownerClient(owningClient),
      virtualKeyboard(isVirtual),
      lastLockedModifiers(wlrKeyboard->modifiers.locked) {
  listen<&KeyboardState::onKey>(&keyboard->events.key, key);
  listen<&KeyboardState::onModifiers>(&keyboard->events.modifiers, modifiers);
  listen<&KeyboardState::onDestroy>(&keyboard->base.events.destroy, destroy);
}

void WaylandCompositor::Impl::KeyboardState::listenForKeymap() {
  // Virtual keyboards publish their keymap after creation. Restore focus
  // after the seat's keymap listener has sent it, before the first key arrives.
  listen<&KeyboardState::onKeymap>(&keyboard->events.keymap, keymap);
}

bool WaylandCompositor::Impl::KeyboardState::inputMethodBridgeActive() const {
  return impl->activeTextInput && impl->activeTextInput->text &&
         impl->activeTextInput->text->focused_surface && impl->inputMethod &&
         impl->inputMethod->method && impl->inputMethod->method->keyboard_grab;
}

void WaylandCompositor::Impl::KeyboardState::onKey(
    wlr_keyboard_key_event *event) {
  if (!event)
    return;
  if (impl->idleNotify)
    impl->idleNotify->activity();
  auto *desktop = impl->q;
  if (!virtualKeyboard)
    impl->activePhysicalKeyboard = keyboard;
  const bool bridgeActive = inputMethodBridgeActive();
  const bool inputMethodVirtual = impl->inputMethodVirtualKeyboard(this);

  if (virtualKeyboard && !inputMethodVirtual) {
    if (auto *physical = impl->preferredKeyboard();
        physical && physical != keyboard)
      copyKeyboardLocks(keyboard, physical);
  }

  if (inputMethodVirtual) {

    impl->restorePreferredKeyboard();
    if (bridgeActive) {
      if (auto *physical = impl->preferredKeyboard();
          physical && physical != keyboard)
        wlr_seat_keyboard_notify_modifiers(impl->seat, &physical->modifiers);
    }
  } else {
    // Generic virtual keyboards are real input sources. GPU Screen Recorder
    // grabs the evdev keyboard and re-emits it through virtual-keyboard-v1;
    // forcing the stale physical keyboard here loses Ctrl and lock state.
    wlr_seat_set_keyboard(impl->seat, keyboard);
  }

  // Locked: no compositor shortcuts and no input method, only the lock screen.
  if (impl->sessionLocked()) {
    wlr_seat_keyboard_notify_key(impl->seat, event->time_msec, event->keycode,
                                 event->state);
    if (inputMethodVirtual)
      impl->restorePreferredKeyboard();
    return;
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

  // Reserve a quick press-and-release of Meta for the overview without
  // breaking the existing Meta+key compositor shortcuts. The modifier event is
  // still delivered through wl_keyboard.modifiers, so an unbound Meta combo
  // can continue to reach a client even though the bare modifier key event is
  // kept by the compositor.
  if (!virtualKeyboard && !desktop->shortcutCapture_) {
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED && metaKey) {
      const auto modifiers = wlr_keyboard_get_modifiers(keyboard);
      if (!(modifiers & (WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT |
                         WLR_MODIFIER_SHIFT))) {
        metaTapPending = true;
        metaTapKeycode = static_cast<int>(event->keycode);
        return;
      }
    } else if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED && metaTapPending) {
      metaTapPending = false;
    }

    if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
        metaTapKeycode == static_cast<int>(event->keycode)) {
      const bool tapped = metaTapPending;
      metaTapPending = false;
      metaTapKeycode = -1;
      if (tapped)
        desktop->runAction("toggle-overview");
      return;
    }
  }

  if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
      consumedKeys.erase(event->keycode))
    return;

  const bool shortcutPress = !virtualKeyboard &&
                             event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
                             !desktop->shortcutCapture_;
  if (shortcutPress)
    consumedKeys.insert(event->keycode);
  bool handled = false;
  if (shortcutPress) {
    const auto modifiers = wlr_keyboard_get_modifiers(keyboard);
    auto *switcher = desktop->windowSwitcher_.get();
    for (int i = 0; i < count; ++i) {
      const auto symbol = symbols[i];
      const bool tab = symbol == XKB_KEY_Tab || symbol == XKB_KEY_ISO_Left_Tab;
      if (tab && (modifiers & WLR_MODIFIER_ALT) &&
          !(modifiers & (WLR_MODIFIER_CTRL | WLR_MODIFIER_LOGO))) {
        desktop->beginWindowSwitch(modifiers & WLR_MODIFIER_SHIFT ? -1 : 1);
        handled = true;
      } else if (switcher->active()) {
        if (symbol == XKB_KEY_Escape)
          desktop->finishWindowSwitch(false);
        else if (symbol == XKB_KEY_Return)
          desktop->finishWindowSwitch(true);
        else if (symbol == XKB_KEY_Left || symbol == XKB_KEY_Right)
          switcher->step(symbol == XKB_KEY_Left ? -1 : 1);
        else if (symbol == XKB_KEY_Up || symbol == XKB_KEY_Down)
          switcher->step(
              (symbol == XKB_KEY_Up ? -1 : 1) *
              (switcher->scope() == WindowSwitcher::Scope::Workspaces ? 5 : 1));
        handled = symbol != XKB_KEY_Alt_L && symbol != XKB_KEY_Alt_R &&
                  symbol != XKB_KEY_Shift_L && symbol != XKB_KEY_Shift_R &&
                  !metaKey;
      }
    }
  }
  // The overview's own bindings first while it is open, then the normal ones.
  if (!handled && shortcutPress) {
    const uint32_t modifiers =
        bindingModifiers(wlr_keyboard_get_modifiers(keyboard));
    std::vector<const char *> modes;
    if (desktop->overviewOpen())
      modes.push_back("overview");
    modes.push_back("normal");
    for (const char *mode : modes)
      for (int i = 0; i < count && !handled; ++i) {
        const std::string action =
            desktop->keyBindings_->actionFor(mode, symbols[i], modifiers);
        if (!action.empty()) {
          desktop->runAction(action);
          handled = true;
        }
      }
  }

  if (!handled && !virtualKeyboard)
    for (int i = 0; i < count; ++i)
      if (keypadSymbol(symbols[i])) {
        ++desktop->keyboardDiagnostics_.keypadKeyForwards;
        break;
      }

  if (handled) {
    consumedKeys.insert(event->keycode);
    return;
  }

  if (shortcutPress)
    consumedKeys.erase(event->keycode);

  if (bridgeActive && !inputMethodVirtual) {
    auto *grab = impl->inputMethod->method->keyboard_grab;
    if (grab->keyboard != keyboard)
      wlr_input_method_keyboard_grab_v2_set_keyboard(grab, keyboard);
    wlr_input_method_keyboard_grab_v2_send_key(grab, event->time_msec,
                                               event->keycode, event->state);
    return;
  }

  wlr_seat_keyboard_notify_key(impl->seat, event->time_msec, event->keycode,
                               event->state);
  if (inputMethodVirtual)
    impl->restorePreferredKeyboard();
}

void WaylandCompositor::Impl::KeyboardState::onModifiers() {
  auto *desktop = impl->q;
  if (!virtualKeyboard)
    impl->activePhysicalKeyboard = keyboard;

  const bool bridgeActive = inputMethodBridgeActive();
  const bool inputMethodVirtual = impl->inputMethodVirtualKeyboard(this);

  // Only the input method's own virtual keyboard is a return path. Other
  // virtual keyboards (notably gsr-ui virtual keyboard) replace a grabbed
  // hardware stream and must keep their own depressed/latched/locked state.
  if (inputMethodVirtual) {
    impl->restorePreferredKeyboard();
    if (bridgeActive) {
      if (auto *physical = impl->preferredKeyboard();
          physical && physical != keyboard) {
        wlr_seat_keyboard_notify_modifiers(impl->seat, &physical->modifiers);
        return;
      }
    }
    wlr_seat_keyboard_notify_modifiers(impl->seat, &keyboard->modifiers);
    return;
  }

  wlr_seat_set_keyboard(impl->seat, keyboard);

  // wlroots updates xkb_state before emitting this modifiers signal. Track the
  // raw locked mask separately so CapsLock/NumLock transitions remain visible
  // even while an input-method-v2 keyboard grab is active.
  const uint32_t locked = keyboard->modifiers.locked;
  const bool lockedChanged = locked != lastLockedModifiers;
  lastLockedModifiers = locked;

  if (virtualKeyboard) {
    if (auto *physical = impl->preferredKeyboard();
        physical && physical != keyboard)
      copyKeyboardLocks(physical, keyboard);
  }

  if (bridgeActive) {
    auto current = keyboard->modifiers;
    auto *grab = impl->inputMethod->method->keyboard_grab;
    if (grab->keyboard != keyboard)
      wlr_input_method_keyboard_grab_v2_set_keyboard(grab, keyboard);
    wlr_input_method_keyboard_grab_v2_send_modifiers(grab, &current);
    if (lockedChanged) {
      // Lock state belongs to the physical keyboard and must also reach the
      // focused wl_keyboard. Ordinary Shift/Ctrl/Alt remain owned by the IME.
      wlr_seat_keyboard_send_modifiers(impl->seat, &current);
      ++desktop->keyboardDiagnostics_.modifierResends;
    }
  } else {
    wlr_seat_keyboard_notify_modifiers(impl->seat, &keyboard->modifiers);
  }
  auto *switcher = desktop->windowSwitcher_.get();
  if (!virtualKeyboard && switcher->active() &&
      !(wlr_keyboard_get_modifiers(keyboard) &
        (switcher->scope() == WindowSwitcher::Scope::Workspaces
             ? WLR_MODIFIER_LOGO
             : WLR_MODIFIER_ALT)))
    desktop->finishWindowSwitch(true);
}

void WaylandCompositor::Impl::KeyboardState::onKeymap() {
  if (!impl->seat->keyboard_state.focused_surface)
    impl->restoreLayerFocus();
}

void WaylandCompositor::Impl::KeyboardState::onDestroy() {
  Impl *owner = impl;
  if (!virtualKeyboard)
    owner->q->finishWindowSwitch(false);
  if (owner->activePhysicalKeyboard == keyboard)
    owner->activePhysicalKeyboard = nullptr;

  // A hotkey/remapping application may own the physical evdev stream for its
  // whole lifetime. When its generic virtual keyboard disappears, carry its
  // final state back to the matching physical keymap before selecting it
  // again. This clears a Ctrl released through the virtual stream and keeps
  // CapsLock/NumLock in sync across recorder restarts.
  auto *current = owner->seat ? wlr_seat_get_keyboard(owner->seat) : nullptr;
  auto *physical = virtualKeyboard ? owner->preferredKeyboard() : nullptr;
  if (virtualKeyboard && current == keyboard && physical &&
      physical != keyboard) {
    if (!copyKeyboardState(physical, keyboard))
      copyKeyboardLocks(physical, keyboard);
  }

  std::erase(owner->keyboards, this);
  delete this;

  if (physical) {
    wlr_seat_set_keyboard(owner->seat, physical);
    wlr_seat_keyboard_notify_modifiers(owner->seat, &physical->modifiers);
    ++owner->q->keyboardDiagnostics_.modifierResends;
  } else {
    owner->restorePreferredKeyboard();
  }
  owner->updateSeatCapabilities();
}


void WaylandCompositor::applyKeyboardConfiguration() {
  if (d)
    d->applyKeyboardConfig();
}

void WaylandCompositor::resendKeyboardModifiers() {
  if (!d || !d->seat)
    return;
  if (auto *keyboard = wlr_seat_get_keyboard(d->seat)) {
    wlr_seat_keyboard_notify_modifiers(d->seat, &keyboard->modifiers);
    ++keyboardDiagnostics_.modifierResends;
  }
}

void WaylandCompositor::Impl::applyKeyboardConfig() {
  for (auto *state : keyboards) {
    if (!state || state->virtualKeyboard)
      continue;
    std::string error;
    if (!applyKeyboardPreferences(state->keyboard, desktopPreferences(),
                                  &error))
      logWarning("LunaDash keyboard: %s", error.c_str());
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


WaylandCompositor::Impl::InputMethodState::InputMethodState(
    Impl *owner, wlr_input_method_v2 *inputMethod)
    : impl(owner), method(inputMethod) {
  listen<&InputMethodState::onCommit>(&method->events.commit, commit);
  listen<&InputMethodState::onNewPopup>(&method->events.new_popup_surface,
                                        newPopup);
  listen<&InputMethodState::onGrabKeyboard>(&method->events.grab_keyboard,
                                            grabKeyboard);
  listen<&InputMethodState::onDestroy>(&method->events.destroy, destroy);
}

void WaylandCompositor::Impl::InputMethodState::onCommit() {
  if (!impl->activeTextInput || !impl->activeTextInput->text)
    return;
  auto *text = impl->activeTextInput->text;
  const auto &current = method->current;
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

void WaylandCompositor::Impl::InputMethodState::onNewPopup(
    wlr_input_popup_surface_v2 *popup) {
  if (!popup)
    return;
  auto *tree = wlr_scene_subsurface_tree_create(impl->overlayLayer, popup->surface);
  auto *state = new PopupState(impl, popup, tree);
  impl->inputPopups.push_back(state);
  impl->positionInputPopup(state);
}

void WaylandCompositor::Impl::InputMethodState::onGrabKeyboard(
    wlr_input_method_keyboard_grab_v2 *grab) {
  if (!grab)
    return;
  if (auto *keyboard = impl->preferredKeyboard()) {
    wlr_input_method_keyboard_grab_v2_set_keyboard(grab, keyboard);
    auto modifiers = keyboard->modifiers;
    wlr_input_method_keyboard_grab_v2_send_modifiers(grab, &modifiers);
    wlr_seat_set_keyboard(impl->seat, keyboard);
    wlr_seat_keyboard_send_modifiers(impl->seat, &modifiers);
    ++impl->q->keyboardDiagnostics_.modifierResends;
  }
}

void WaylandCompositor::Impl::InputMethodState::onDestroy() {
  Impl *owner = impl;
  if (owner->inputMethod == this)
    owner->inputMethod = nullptr;
  delete this;
  if (auto *keyboard = owner->preferredKeyboard()) {
    wlr_seat_set_keyboard(owner->seat, keyboard);
    wlr_seat_keyboard_notify_modifiers(owner->seat, &keyboard->modifiers);
    ++owner->q->keyboardDiagnostics_.modifierResends;
  }
}

WaylandCompositor::Impl::PopupState::PopupState(
    Impl *owner, wlr_input_popup_surface_v2 *inputPopup, wlr_scene_tree *tree)
    : impl(owner), popup(inputPopup), sceneTree(tree) {
  listen<&PopupState::onDestroy>(&popup->events.destroy, destroy);
}

void WaylandCompositor::Impl::PopupState::onDestroy() {
  std::erase(impl->inputPopups, this);
  delete this;
}

WaylandCompositor::Impl::TextInputState::TextInputState(
    Impl *owner, wlr_text_input_v3 *textInput)
    : impl(owner), text(textInput) {
  listen<&TextInputState::onEnable>(&text->events.enable, enable);
  listen<&TextInputState::onCommit>(&text->events.commit, commit);
  listen<&TextInputState::onDisable>(&text->events.disable, disable);
  listen<&TextInputState::onDestroy>(&text->events.destroy, destroy);
}

void WaylandCompositor::Impl::TextInputState::onEnable() {
  if (text->focused_surface)
    impl->syncTextInputToMethod(this, true);
}

void WaylandCompositor::Impl::TextInputState::onCommit() {
  if (impl->activeTextInput != this)
    return;
  impl->syncTextInputToMethod(this, false);
  for (auto *popup : impl->inputPopups)
    impl->positionInputPopup(popup);
}

void WaylandCompositor::Impl::TextInputState::onDisable() {
  impl->deactivateTextInput(this);
}

void WaylandCompositor::Impl::TextInputState::onDestroy() {
  impl->deactivateTextInput(this);
  std::erase(impl->textInputs, this);
  delete this;
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
  if (activePhysicalKeyboard)
    return activePhysicalKeyboard;
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
