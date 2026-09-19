#pragma once
#include <QWidget>
#include <functional>
namespace LunaDash {
QWidget *createWelcome(const std::function<void(const QString &)> &launch);
}
