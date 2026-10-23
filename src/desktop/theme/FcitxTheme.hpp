#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace LunaDash {

bool synchronizeFcitxTheme(const nlohmann::json &palette, std::string *error);

} // namespace LunaDash
