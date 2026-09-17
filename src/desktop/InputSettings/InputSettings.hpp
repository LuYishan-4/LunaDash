#pragma once
#include <QJsonObject>
class QWaylandSeat;
namespace LuDash {
void setNativeKeyboardInputEnabled(bool enabled);
void applyKeyboardPreferences(QWaylandSeat *seat, const QJsonObject &preferences);
}
