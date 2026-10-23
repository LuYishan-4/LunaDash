#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace LunaDash {
nlohmann::json wallpaperAction(const std::vector<std::string> &arguments);
} // namespace LunaDash
