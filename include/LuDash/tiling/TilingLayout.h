#pragma once
#include <QRect>
#include <QList>
namespace LuDash {
QList<QRect> tileRectangles(QRect area, int count, double masterRatio = 0.56, int gap = 12);
}
