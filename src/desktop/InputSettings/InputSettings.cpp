#include "desktop/InputSettings/InputSettings.hpp"

#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/QWaylandSeat>

namespace LuDash {
namespace {
QString xkbLayoutForPreference(const QString &layout) {
  // Taiwan PC keyboards normally use US physical key positions. Traditional
  // Chinese composition is handled by Fcitx rather than by the XKB "tw" map.
  return layout == QStringLiteral("tw") ? QStringLiteral("us") : layout;
}
} // namespace

void applyKeyboardPreferences(QWaylandSeat *seat,
                              const QJsonObject &preferences) {
  if (!seat || !seat->keyboard())
    return;

  auto *keymap = seat->keymap();
  keymap->setRules(QStringLiteral("evdev"));
  keymap->setModel(QStringLiteral("pc105"));

  const QString preferenceLayout =
      preferences.value("keyboardLayout").toString(QStringLiteral("us"));
  keymap->setLayout(xkbLayoutForPreference(preferenceLayout));

  // Wayland clients receive repeat information from wl_keyboard. Qt's
  // QWaylandQuickItem forwards the physical QKeyEvent once and
  // QWaylandSeat::sendFullKeyEvent deliberately ignores host auto-repeat
  // events, so the client owns key repetition from this rate/delay pair.
  constexpr quint32 kRepeatRate = 25;
  constexpr quint32 kRepeatDelay = 600;
  seat->keyboard()->setRepeatRate(kRepeatRate);
  seat->keyboard()->setRepeatDelay(kRepeatDelay);
}
} // namespace LuDash
