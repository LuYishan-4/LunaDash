#pragma once

#include <nlohmann/json.hpp>

namespace LunaDash {

nlohmann::json synchronizeApplicationTheme(
    const nlohmann::json &preferences, const nlohmann::json &palette);
// The buttons GTK applications put on their own titlebars: minimize, maximize
// and close while windows stack, close alone while they tile.
void synchronizeWindowButtons(bool stacking);

} // namespace LunaDash
