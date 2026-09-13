#include <LuDash/input_settings/InputSettings.h>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/QWaylandKeyboard>
namespace LuDash {
void applyKeyboardPreferences(QWaylandSeat* seat, const QJsonObject& preferences) {
    if (!seat || !seat->keyboard()) return;
    seat->keymap()->setLayout(preferences.value("keyboardLayout").toString());
    seat->keyboard()->setRepeatRate(static_cast<quint32>(preferences.value("keyRepeatRate").toInt()));
    seat->keyboard()->setRepeatDelay(static_cast<quint32>(preferences.value("keyRepeatDelay").toInt()));
}
}
