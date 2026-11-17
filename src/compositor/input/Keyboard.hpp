#pragma once
#include <nlohmann/json.hpp>
#include <string>
struct wlr_keyboard;
struct wlr_seat;

namespace LunaDash {
bool applyKeyboardPreferences(wlr_keyboard *keyboard,
                              const nlohmann::json &preferences,
                              std::string *error = nullptr);

bool sendEditingKeyChord(wlr_seat *seat, const std::string &action,
                         std::string *error);
} // namespace LunaDash
