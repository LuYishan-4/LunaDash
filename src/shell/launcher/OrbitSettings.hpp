#pragma once
#include <QJsonObject>
#include <QString>

namespace LunaDash {
QJsonObject orbitSettings();
bool saveOrbitSettings(const QByteArray &text, QString *error);
bool resetOrbitSettings(QString *error);
} // namespace LunaDash
