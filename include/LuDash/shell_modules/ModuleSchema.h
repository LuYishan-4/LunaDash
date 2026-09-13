#pragma once
#include <QJsonObject>
#include <QStringList>
namespace LuDash {
QStringList shellModuleIds();
QJsonObject defaultModuleDocument();
bool validateModuleDocument(const QByteArray& text, QJsonObject* normalized, QString* error);
}
