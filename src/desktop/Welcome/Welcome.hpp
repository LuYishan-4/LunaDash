#pragma once
#include <QWidget>
#include <functional>
namespace LuDash {
QWidget* createWelcome(const std::function<void(const QString&)>& launch);
}
