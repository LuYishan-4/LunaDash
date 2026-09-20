#pragma once
#include <QJsonObject>
#include <QString>

namespace LunaDash {
QString extensionConfigurationPath();
QJsonObject readExtensionConfiguration(QString *error = nullptr);
bool saveExtensionConfiguration(const QByteArray &json, QString *error);
QJsonObject configuredBuiltinSettings(const QString &target);
QJsonObject configuredBuiltinSettings(const QString &target,
                                      const QJsonObject &document);
} // namespace LunaDash
