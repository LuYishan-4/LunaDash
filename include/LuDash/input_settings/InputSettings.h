#pragma once
#include <QJsonObject>
class QWaylandSeat;
namespace LuDash { void applyKeyboardPreferences(QWaylandSeat* seat, const QJsonObject& preferences); }
