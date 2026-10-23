#pragma once
#include <nlohmann/json.hpp>

#include <string>

namespace LunaDash {
nlohmann::json desktopPreferences();
void ensureLegacyDesktopSettingsImported();
bool updateDesktopPreferences(const nlohmann::json &changes,
                              std::string *error);
bool setupComplete();
bool setSetupComplete(bool complete, std::string *error);
} // namespace LunaDash
