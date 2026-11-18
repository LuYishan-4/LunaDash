#pragma once

#include "core/geometry/Geometry.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

nlohmann::json defaultDesktopPreferenceValues();

// "WIDTHxHEIGHT" between 320x240 and 7680x4320, as virtual output sizes are
// written in preferences and in the desktop-size command.
std::optional<Size> parseVirtualOutputSize(std::string_view text);
// The size of an output that advertises no modes (nested windows, headless
// outputs) until something resizes it.
Size virtualOutputSize(const nlohmann::json &preferences);
// The sizes desktop-size and the nested Settings → Display modes offer.
std::vector<Size> virtualOutputSizes(const nlohmann::json &preferences);
nlohmann::json loadDesktopPreferences();
// Changes whenever the stored preferences file does (written by any process),
// so results derived from it can be cached; 0 while there is no usable file.
std::uint64_t desktopPreferenceStoreGeneration();
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
