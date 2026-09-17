#pragma once
#include <QJsonArray>
#include <QStringList>
namespace LuDash {
QJsonArray systemSettingsTools();
bool systemSettingsToolUsesHost(const QString& id);
QStringList systemSettingsCommand(const QString& id);
}
