#pragma once
#include <QJsonObject>
class QWindow;
namespace LunaDash {
QJsonObject describeDisplay(const QWindow *window);
bool resizeNestedDesktop(QWindow *window, const QString &preset,
                         QString *error);
} // namespace LunaDash
