#pragma once
#include <QWidget>
#include <functional>
namespace LuDash {
QWidget* createNotes(std::function<bool()>& canClose);
}
