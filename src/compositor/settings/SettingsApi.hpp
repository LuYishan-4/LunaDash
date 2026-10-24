#pragma once
#include "core/json/JsonAccess.hpp"

#include <functional>
#include <string>
namespace LunaDash {
class PluginManager;
class ShellModules;
// Native effects and visual plugins expose the same settings contract.
Json settingsApiTargets(const Json &extensions, const Json &modules,
                        const Json &layout);
bool updateSettingsApi(
    PluginManager &plugins, ShellModules &modules, const Json &layout,
    const Json &request,
    const std::function<bool(const Json &, std::string *)> &setLayout,
    std::string *error);
}
