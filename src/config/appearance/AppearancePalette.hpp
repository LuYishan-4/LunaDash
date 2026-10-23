#pragma once

#include <nlohmann/json.hpp>

namespace LunaDash {
nlohmann::json appearancePalette(const nlohmann::json &preferences);
bool appearanceIsDark(const nlohmann::json &preferences);
} // namespace LunaDash
