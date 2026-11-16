#include "compositor/input/Keyboard.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "config/desktop/PreferenceKeys.hpp"
#include "desktop/input/InputSettings.hpp"

#include <chrono>
#include <cstdint>
#include <string_view>

namespace LunaDash {
bool applyKeyboardPreferences(wlr_keyboard *keyboard,
                              const nlohmann::json &preferences,
                              std::string *error) {
  if (!keyboard) {
    if (error)
      *error = "No wlroots keyboard is available.";
    return false;
  }

  const std::string layout = keyboardLayoutPreference(
      preferenceValue(preferences, Preference::keyboardLayout));
  xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!context) {
    if (error)
      *error = "Could not create the XKB context.";
    return false;
  }

  const xkb_rule_names names{
      .rules = "evdev",
      .model = "pc105",
      .layout = layout.c_str(),
      .variant = nullptr,
      .options = nullptr,
  };
  xkb_keymap *keymap =
      xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!keymap) {
    xkb_context_unref(context);
    if (error)
      *error = "Could not compile the XKB keymap.";
    return false;
  }

  const bool ok = wlr_keyboard_set_keymap(keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  if (!ok) {
    if (error)
      *error = "wlroots rejected the XKB keymap.";
    return false;
  }

  wlr_keyboard_set_repeat_info(keyboard, keyboardRepeatRate(),
                               keyboardRepeatDelay());
  return true;
}


namespace {
// XKB key names of the editing actions; the chord is LCTL + key.
const char *editingKeyName(std::string_view action) {
  return action == "copy"        ? "AC03"
         : action == "paste"     ? "AB04"
         : action == "cut"       ? "AB02"
         : action == "selectAll" ? "AC01"
                                 : nullptr;
}

// XKB keycodes are evdev codes offset by 8.
constexpr xkb_keycode_t xkbEvdevOffset = 8;
} // namespace

bool sendEditingKeyChord(wlr_seat *seat, const std::string &action,
                         std::string *error) {
  if (!seat || !seat->keyboard_state.focused_surface) {
    *error = "No application has keyboard focus.";
    return false;
  }
  auto *keyboard = wlr_seat_get_keyboard(seat);
  if (!keyboard || !keyboard->keymap) {
    *error = "No wlroots keyboard is available.";
    return false;
  }
  const char *name = editingKeyName(action);
  if (!name) {
    *error = "Unknown key action.";
    return false;
  }
  const xkb_keycode_t ctrl = xkb_keymap_key_by_name(keyboard->keymap, "LCTL");
  const xkb_keycode_t key = xkb_keymap_key_by_name(keyboard->keymap, name);
  if (!ctrl || !key) {
    *error = "Keymap does not contain the requested keys.";
    return false;
  }
  const auto now = static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count() &
      0xffffffff);
  wlr_seat_keyboard_notify_key(seat, now, ctrl - xkbEvdevOffset,
                               WL_KEYBOARD_KEY_STATE_PRESSED);
  wlr_seat_keyboard_notify_key(seat, now, key - xkbEvdevOffset,
                               WL_KEYBOARD_KEY_STATE_PRESSED);
  wlr_seat_keyboard_notify_key(seat, now, key - xkbEvdevOffset,
                               WL_KEYBOARD_KEY_STATE_RELEASED);
  wlr_seat_keyboard_notify_key(seat, now, ctrl - xkbEvdevOffset,
                               WL_KEYBOARD_KEY_STATE_RELEASED);
  return true;
}

} // namespace LunaDash
