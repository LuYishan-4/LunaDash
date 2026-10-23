#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace LunaDash {

nlohmann::json orbitSettings();
bool saveOrbitSettings(std::string_view text, std::string *error);
bool resetOrbitSettings(std::string *error);

} // namespace LunaDash
