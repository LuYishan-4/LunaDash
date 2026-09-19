#include "desktop/input/InputSettings.hpp"
#include "core/Defines.hpp"

#include <QStringList>

#if LUDASH_HAS_XKBREGISTRY
#include <xkbcommon/xkbregistry.h>
#endif

namespace LunaDash {
namespace {

QString validatedLayout(const QString &requested) {
#if LUDASH_HAS_XKBREGISTRY
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
  return validatedLayout(preferences.value(QStringLiteral("keyboardLayout"))
                             .toString(QStringLiteral("us")));
}

int keyboardRepeatRate() { return 25; }
int keyboardRepeatDelay() { return 600; }

} // namespace LunaDash
