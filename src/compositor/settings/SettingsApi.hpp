#pragma once
#include "core/json/JsonAccess.hpp"

#include <functional>
#include <string>
namespace LunaDash {
class PluginManager;
// Built-in features, plugins and the window layout expose the same settings contract.
Json settingsApiTargets(const Json &extensions, const Json &layout);
bool updateSettingsApi(
    PluginManager &plugins, const Json &layout,
    const Json &request,
    const std::function<bool(const Json &, std::string *)> &setLayout,
    std::string *error);
}
