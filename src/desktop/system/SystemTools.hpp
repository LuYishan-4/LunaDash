#pragma once
#include <QJsonArray>
#include <QStringList>
namespace LunaDash {
QJsonArray systemSettingsTools();
bool systemSettingsToolUsesHost(const QString &id);
QStringList systemSettingsCommand(const QString &id);
} // namespace LunaDash
