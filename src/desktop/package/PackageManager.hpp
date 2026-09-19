#pragma once
#include <QStringList>
class QWidget;
namespace LunaDash {
QStringList packageTransactionArguments(const QString &operation,
                                        const QString &package = {});
QWidget *createPackageManager();
} // namespace LunaDash
