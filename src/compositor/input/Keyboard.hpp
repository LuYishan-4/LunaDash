#pragma once
#include <QJsonObject>
#include <QString>
struct wlr_keyboard;

namespace LunaDash {
bool applyKeyboardPreferences(wlr_keyboard *keyboard,
                              const QJsonObject &preferences,
                              QString *error = nullptr);
} // namespace LunaDash
