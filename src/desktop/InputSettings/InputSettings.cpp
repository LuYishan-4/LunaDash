#include "desktop/InputSettings/InputSettings.hpp"

#include <QStringList>

#ifdef LUDASH_USE_XKBREGISTRY
#include <xkbcommon/xkbregistry.h>
#endif

#include "compositor/wlroots/WlrootsKeyboardHeaders.hpp"

namespace LuDash {
namespace {

QString validatedLayout(const QString &requested) {
#ifdef LUDASH_USE_XKBREGISTRY
  rxkb_context *context = rxkb_context_new(RXKB_CONTEXT_NO_FLAGS);
  if (!context)
    return QStringLiteral("us");
  if (!rxkb_context_parse_default_ruleset(context)) {
    rxkb_context_unref(context);
    return QStringLiteral("us");
  }

  QString result = QStringLiteral("us");
  for (rxkb_layout *layout = rxkb_layout_first(context); layout;
       layout = rxkb_layout_next(layout)) {
    const char *name = rxkb_layout_get_name(layout);
    const char *variant = rxkb_layout_get_variant(layout);
    if (name && !variant && QString::fromUtf8(name) == requested) {
      result = QString::fromUtf8(name);
      break;
    }
  }
  rxkb_context_unref(context);
  return result;
#else
  static const QStringList supported{QStringLiteral("us"), QStringLiteral("gb"),
                                     QStringLiteral("de"), QStringLiteral("fr"),
                                     QStringLiteral("es")};
  return supported.contains(requested) ? requested : QStringLiteral("us");
#endif
}

} // namespace

QString keyboardLayoutPreference(const QJsonObject &preferences) {
  return validatedLayout(
      preferences.value(QStringLiteral("keyboardLayout"))
          .toString(QStringLiteral("us")));
}

int keyboardRepeatRate() { return 25; }
int keyboardRepeatDelay() { return 600; }

bool applyKeyboardPreferences(wlr_keyboard *keyboard,
                              const QJsonObject &preferences,
                              QString *error) {
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

} // namespace LuDash
