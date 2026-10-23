#pragma once
#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace LunaDash {
nlohmann::json desktopPreferences();
nlohmann::json loadDisplayConfiguration(std::string_view output,
                                       std::string *error);
nlohmann::json loadWindowLayoutSettings(std::string_view templateKey,
                                        const nlohmann::json &schema,
                                        std::string *error);
void ensureLegacyDesktopSettingsImported();
bool updateDesktopPreferences(const nlohmann::json &changes,
                              std::string *error);
bool setupComplete();
bool setSetupComplete(bool complete, std::string *error);
} // namespace LunaDash
