#pragma once
#include <QJsonObject>
#include <QString>
namespace LunaDash {
QJsonObject desktopPreferences();
bool updateDesktopPreferences(const QJsonObject &changes, QString *error);
bool setupComplete();
void setSetupComplete(bool complete);
} // namespace LunaDash
