#pragma once
#include <nlohmann/json.hpp>
#include <string>
struct wlr_keyboard;
struct wlr_seat;

namespace LunaDash {
bool applyKeyboardPreferences(wlr_keyboard *keyboard,
                              const nlohmann::json &preferences,
                              std::string *error = nullptr);
// Sends Ctrl+<key> for an editing action (copy, paste, cut, selectAll) to the
// surface with keyboard focus.
bool sendEditingKeyChord(wlr_seat *seat, const std::string &action,
                         std::string *error);
} // namespace LunaDash
