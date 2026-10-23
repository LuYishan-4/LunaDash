#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <string_view>

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
nlohmann::json loadWindowLayoutSettings(std::string_view templateKey);
bool hasStoredWindowLayoutSettings(std::string_view templateKey);
bool saveWindowLayoutSettings(std::string_view templateKey,
                              const nlohmann::json &settings,
                              std::string *error);
nlohmann::json loadDisplaySettings(std::string_view output);
bool saveDisplaySettings(std::string_view output,
                         const nlohmann::json &settings, std::string *error);
bool isDesktopSetupComplete();
bool setDesktopSetupComplete(bool complete, std::string *error);

} // namespace LunaDash
