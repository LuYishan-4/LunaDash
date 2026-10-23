#include "config/desktop/DesktopPreferenceStore.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  char directoryTemplate[] = "/tmp/ludash-preferences-XXXXXX";
  const char *directory = ::mkdtemp(directoryTemplate);
  assert(directory);
  assert(::setenv("XDG_CONFIG_HOME", directory, 1) == 0);
  assert(::setenv("HOME", directory, 1) == 0);

  const auto defaults = LunaDash::defaultDesktopPreferenceValues();
  assert(defaults.at("themeMode") == "dark");
  assert(defaults.at("workspaceCount") == 10);
  assert(defaults.at("startupApps").empty());

  std::string error;
  const std::string wallpaperFirstDirectory =
      std::string(directory) + "/wallpaper-first";
  assert(::setenv("XDG_CONFIG_HOME", wallpaperFirstDirectory.c_str(), 1) == 0);
  assert(LunaDash::saveWallpaperSettings(
      {{"mode", "shader"}, {"shader", 1}, {"revision", 4}}, &error));
  assert(LunaDash::importLegacyDesktopPreferences(
      {{"themeMode", "light"}, {"workspaceCount", 3}}, false, &error));
  assert(LunaDash::loadDesktopPreferences().at("themeMode") == "light");
  assert(LunaDash::loadWallpaperSettings().at("mode") == "shader");
  assert(LunaDash::loadWallpaperSettings().at("revision") == 4);
  assert(::setenv("XDG_CONFIG_HOME", directory, 1) == 0);

  assert(LunaDash::importLegacyDesktopPreferences(
      {{"themeMode", "light"},
       {"workspaceCount", 4},
       {"startupApps", {"files", "welcome"}}},
      true, &error));
  auto preferences = LunaDash::loadDesktopPreferences();
  assert(preferences.at("themeMode") == "light");
  assert(preferences.at("workspaceCount") == 4);
  assert(preferences.at("startupApps") == nlohmann::json({"files", "welcome"}));
  assert(LunaDash::isDesktopSetupComplete());

  assert(LunaDash::importLegacyWallpaperSettings(
      {{"mode", "shader"},
       {"image", "/tmp/selected-wallpaper.png"},
       {"shader", 1},
       {"history", {"/tmp/previous-wallpaper.png"}},
       {"favorites", {"/tmp/favorite-wallpaper.png"}},
       {"revision", 12},
       {"libraryRefresh", "legacy-refresh"}},
      &error));
  assert(LunaDash::saveWallpaperSettings({{"shader", 1.0}}, &error));
  auto wallpaper = LunaDash::loadWallpaperSettings();
  assert(wallpaper.at("mode") == "shader");
  assert(wallpaper.at("shader") == 1);
  assert(wallpaper.at("revision") == 12);
  assert(wallpaper.at("history").size() == 1);
  assert(wallpaper.at("favorites").size() == 1);
  assert(!LunaDash::saveWallpaperSettings({{"mode", "unknown"}, {"shader", 0}},
                                          &error));
  assert(!LunaDash::saveWallpaperSettings({{"shader", 1.5}}, &error));
  assert(LunaDash::loadWallpaperSettings() == wallpaper);
  assert(LunaDash::saveWallpaperSettings(
      {{"mode", "image"}, {"shader", 0}, {"revision", 13}}, &error));
  wallpaper = LunaDash::loadWallpaperSettings();
  assert(wallpaper.at("mode") == "image");
  assert(wallpaper.at("revision") == 13);

  const auto beforeInvalidUpdate = preferences;
  assert(!LunaDash::saveDesktopPreferences(
      {{"themeMode", "dark"}, {"eyeCareTemperature", 0}}, &error));
  assert(!LunaDash::saveDesktopPreferences(
      {{"weatherLocation", std::string(81, 'x')}}, &error));
  assert(!LunaDash::saveDesktopPreferences(
      {{"proxyHttp", "http://proxy.invalid:bad"}}, &error));
  assert(LunaDash::loadDesktopPreferences() == beforeInvalidUpdate);

  assert(LunaDash::saveDesktopPreferences(
      {{"themeMode", "dark"}, {"weatherLatitude", 37.5}}, &error));
  assert(LunaDash::saveDesktopPreferences({{"language", "zh_TW"}}, &error));
  assert(!LunaDash::saveDesktopPreferences({{"language", "xx_XX"}}, &error));
  preferences = LunaDash::loadDesktopPreferences();
  assert(preferences.at("themeMode") == "dark");
  assert(preferences.at("weatherLatitude") == 37.5);
  assert(preferences.at("language") == "zh_TW");

  assert(LunaDash::importLegacyShortcutBindings(
      {{"closeWindow", "Meta+Q"}, {"invalid", false}}, &error));
  assert(LunaDash::loadShortcutBindings() ==
         nlohmann::json({{"closeWindow", "Meta+Q"}}));
  assert(LunaDash::loadDesktopPreferences().at("themeMode") == "dark");
  assert(LunaDash::loadWallpaperSettings().at("mode") == "image");
  assert(!LunaDash::saveShortcutBindings({{"closeWindow", 42}}, &error));
  assert(LunaDash::resetDesktopPreferences(&error));
  assert(LunaDash::legacyLanguagePreferenceIgnored());
  assert(LunaDash::loadDesktopPreferences().at("themeMode") == "dark");
  assert(LunaDash::loadDesktopPreferences().at("weatherLatitude") == 0.0);
  assert(LunaDash::isDesktopSetupComplete());
  assert(LunaDash::loadWallpaperSettings().at("mode") == "image");
  assert(LunaDash::loadShortcutBindings().at("closeWindow") == "Meta+Q");

  const std::filesystem::path file =
      std::filesystem::path(directory) / "LunaDash/desktop-preferences.json";
  struct stat status{};
  assert(::stat(file.c_str(), &status) == 0);
  assert((status.st_mode & 0777) == 0600);
  assert(LunaDash::setDesktopSetupComplete(false, &error));
  assert(!LunaDash::isDesktopSetupComplete());

  std::filesystem::remove_all(directory);
}
