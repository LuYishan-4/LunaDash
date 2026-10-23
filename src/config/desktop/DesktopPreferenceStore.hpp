#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

namespace LunaDash {

nlohmann::json defaultDesktopPreferenceValues();
nlohmann::json loadDesktopPreferences();
bool saveDesktopPreferences(const nlohmann::json &changes, std::string *error);
bool resetDesktopPreferences(std::string *error);
bool hasStoredDesktopPreferences();
bool legacyLanguagePreferenceIgnored();
bool importLegacyDesktopPreferences(const nlohmann::json &values,
                                    bool setupComplete, std::string *error);
nlohmann::json loadWallpaperSettings();
bool saveWallpaperSettings(const nlohmann::json &changes, std::string *error);
bool hasStoredWallpaperSettings();
bool importLegacyWallpaperSettings(const nlohmann::json &values,
                                   std::string *error);
nlohmann::json loadShortcutBindings();
bool saveShortcutBindings(const nlohmann::json &bindings, std::string *error);
bool hasStoredShortcutBindings();
bool importLegacyShortcutBindings(const nlohmann::json &bindings,
                                  std::string *error);
bool isDesktopSetupComplete();
bool setDesktopSetupComplete(bool complete, std::string *error);

} // namespace LunaDash
