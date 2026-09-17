#pragma once
#include <QJsonObject>
#include <QString>
namespace LuDash {
QJsonObject desktopPreferences();
bool updateDesktopPreferences(const QJsonObject& changes, QString* error);
bool setupComplete();
void setSetupComplete(bool complete);
}
