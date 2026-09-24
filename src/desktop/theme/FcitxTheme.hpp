#pragma once
#include <QJsonObject>
#include <QString>
namespace LunaDash {
bool synchronizeFcitxTheme(const QJsonObject &palette, QString *error);
} // namespace LunaDash
