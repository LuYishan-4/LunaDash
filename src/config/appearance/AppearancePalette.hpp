#pragma once

#include <QJsonObject>

namespace LunaDash {
QJsonObject appearancePalette(const QJsonObject &preferences);
bool appearanceIsDark(const QJsonObject &preferences);
} // namespace LunaDash
