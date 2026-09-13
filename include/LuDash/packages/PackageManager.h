#pragma once
#include <QStringList>
class QWidget;
namespace LuDash {
QStringList packageTransactionArguments(const QString& operation, const QString& package = {});
QWidget* createPackageManager();
}
