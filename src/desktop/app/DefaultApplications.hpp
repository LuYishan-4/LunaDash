#pragma once
#include <QJsonObject>
#include <QStringList>
namespace LunaDash {
QJsonObject defaultApplications();
bool setDefaultApplications(const QJsonObject &changes, QString *error);
QStringList defaultApplicationCommand(const QString &role, QString *error);
QStringList konsoleCommand();
} // namespace LunaDash
