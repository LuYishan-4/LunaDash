#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace LunaDash {
QJsonArray extensionTargets();
QJsonObject extensionTarget(const QString &id);
bool validateExtensionSettings(const QJsonObject &schema,
                               const QJsonObject &settings, QString *error);
QJsonObject extensionDefaults(const QJsonObject &schema);
} // namespace LunaDash
