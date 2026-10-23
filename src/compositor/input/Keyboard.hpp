#pragma once
#include <nlohmann/json.hpp>
#include <string>
struct wlr_keyboard;

namespace LunaDash {
bool applyKeyboardPreferences(wlr_keyboard *keyboard,
                              const nlohmann::json &preferences,
                              std::string *error = nullptr);
} // namespace LunaDash
