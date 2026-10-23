#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace LunaDash {

nlohmann::json appearancePresets();
bool saveAppearancePreset(std::string_view name, std::string *error);
bool applyAppearancePreset(std::string_view name, std::string *error);
bool deleteAppearancePreset(std::string_view name, std::string *error);

} // namespace LunaDash
