#pragma once
#include <QJsonObject>
class QWaylandSeat;
namespace LuDash {
void setKeyboardLedControlEnabled(bool enabled);
void handleKeyboardLockKey(int qtKey, bool pressed, bool autoRepeat);
void applyKeyboardPreferences(QWaylandSeat *seat, const QJsonObject &preferences);
}
