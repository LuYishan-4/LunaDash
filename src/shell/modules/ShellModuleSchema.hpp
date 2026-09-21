#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
namespace LunaDash {
QJsonArray shellModuleDescriptors();
QJsonObject shellModuleDescriptor(const QString &id);
QStringList shellModuleIds();
QJsonObject defaultModuleDocument();
bool validateModuleDocument(const QByteArray &text, QJsonObject *normalized,
                            QString *error);
} // namespace LunaDash
