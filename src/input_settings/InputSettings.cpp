#include <LuDash/input_settings/InputSettings.h>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/QWaylandSeat>
namespace LuDash {
void applyKeyboardPreferences(QWaylandSeat *seat,
                              const QJsonObject &preferences) {
  if (!seat || !seat->keyboard())
    return;
  auto *keymap = seat->keymap();
  // Explicit rules/model keep the evdev keymap complete, including the NumLock
  // modifier, regardless of the host's own keymap defaults.
  keymap->setRules(QStringLiteral("evdev"));
  keymap->setModel(QStringLiteral("pc105"));
  keymap->setLayout(preferences.value("keyboardLayout").toString());
  seat->keyboard()->setRepeatRate(
      static_cast<quint32>(preferences.value("keyRepeatRate").toInt()));
  seat->keyboard()->setRepeatDelay(
      static_cast<quint32>(preferences.value("keyRepeatDelay").toInt()));
}
} // namespace LuDash
