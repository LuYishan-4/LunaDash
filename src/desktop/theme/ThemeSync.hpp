#pragma once

#include <nlohmann/json.hpp>

namespace LunaDash {

nlohmann::json synchronizeApplicationTheme(
    const nlohmann::json &preferences, const nlohmann::json &palette);

} // namespace LunaDash
