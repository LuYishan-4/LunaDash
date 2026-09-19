#pragma once
#include <QStringList>
namespace LunaDash {
bool validFileName(const QString &name);
QString performFileOperation(const QString &operation, const QStringList &paths,
                             const QString &destination);
} // namespace LunaDash
