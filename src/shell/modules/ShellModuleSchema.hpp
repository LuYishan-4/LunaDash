#pragma once
#include <QJsonObject>
#include <QStringList>
namespace LunaDash {
QStringList shellModuleIds();
QJsonObject defaultModuleDocument();
bool validateModuleDocument(const QByteArray &text, QJsonObject *normalized,
                            QString *error);
} // namespace LunaDash
