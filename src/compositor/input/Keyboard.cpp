#include "compositor/input/Keyboard.hpp"
#include "compositor/wayland/wlroots/WlrootsKeyboardHeaders.hpp"
#include "desktop/input/InputSettings.hpp"

namespace LunaDash {
bool applyKeyboardPreferences(wlr_keyboard *keyboard,
                              const QJsonObject &preferences, QString *error) {
  if (!keyboard) {
    if (error)
      *error = QStringLiteral("No wlroots keyboard is available.");
    return false;
  }

  const QByteArray layout = keyboardLayoutPreference(preferences).toUtf8();
  xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!context) {
    if (error)
      *error = QStringLiteral("Could not create the XKB context.");
    return false;
  }

  const xkb_rule_names names{
      .rules = "evdev",
      .model = "pc105",
      .layout = layout.constData(),
      .variant = nullptr,
      .options = nullptr,
  };
  xkb_keymap *keymap =
      xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!keymap) {
    xkb_context_unref(context);
    if (error)
      *error = QStringLiteral("Could not compile the XKB keymap.");
    return false;
  }

  const bool ok = wlr_keyboard_set_keymap(keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  if (!ok) {
    if (error)
      *error = QStringLiteral("wlroots rejected the XKB keymap.");
    return false;
  }

  wlr_keyboard_set_repeat_info(keyboard, keyboardRepeatRate(),
                               keyboardRepeatDelay());
  return true;
}

} // namespace LunaDash
