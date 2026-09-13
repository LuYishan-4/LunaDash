#pragma once
#include <QWidget>
#include <functional>
namespace LuDash {
QWidget* createSettings(bool tiled = true, const std::function<void(bool)>& setTiled = {});
}
