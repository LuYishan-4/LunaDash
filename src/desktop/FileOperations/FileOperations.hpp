#pragma once
#include <QStringList>
namespace LuDash {
bool validFileName(const QString& name);
QString performFileOperation(const QString& operation, const QStringList& paths, const QString& destination);
}
