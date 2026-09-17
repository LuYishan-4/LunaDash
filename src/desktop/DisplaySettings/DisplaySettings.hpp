#pragma once
#include <QJsonObject>
class QWindow;
namespace LuDash {
QJsonObject describeDisplay(const QWindow* window);
bool resizeNestedDesktop(QWindow* window, const QString& preset, QString* error);
}
