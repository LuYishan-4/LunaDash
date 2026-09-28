#pragma once
#include <QJsonObject>
#include <QString>

namespace LunaDash {
QJsonObject synchronizeApplicationTheme(const QJsonObject &preferences,
                                        const QJsonObject &palette);
} // namespace LunaDash
