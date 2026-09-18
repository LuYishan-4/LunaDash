#pragma once

#include <QJsonObject>
#include <QString>

struct wlr_keyboard;

namespace LuDash {

QString keyboardLayoutPreference(const QJsonObject &preferences);
int keyboardRepeatRate();
int keyboardRepeatDelay();
bool applyKeyboardPreferences(wlr_keyboard *keyboard,
                              const QJsonObject &preferences,
                              QString *error = nullptr);

} // namespace LuDash
