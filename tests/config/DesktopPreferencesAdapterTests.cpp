#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QSettings>
#include <QTemporaryDir>

#include <cassert>
#include <filesystem>

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  QTemporaryDir temporary;
  assert(temporary.isValid());
  qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8());
  QCoreApplication::setOrganizationName("LunaDash");
  QCoreApplication::setApplicationName("LunaDash");

  {
    QSettings legacy;
    legacy.setValue("desktop/themeMode", "light");
    legacy.setValue("desktop/workspaceCount", 4);
    legacy.setValue("desktop/startupApps", QStringList{"files", "welcome"});
    legacy.setValue("session/setupComplete", true);
    legacy.setValue("appearance/wallpaperMode", "shader");
    legacy.setValue("appearance/wallpaperImage", "/tmp/selected-wallpaper.png");
    legacy.setValue("appearance/wallpaper", 1);
    legacy.setValue("appearance/wallpaperHistory",
                    QStringList{"/tmp/wallpaper-one.jpg"});
    legacy.setValue("appearance/wallpaperFavorites",
                    QStringList{"/tmp/favorite-wallpaper.jpg"});
    legacy.setValue("appearance/wallpaperRevision", qulonglong{8});
    legacy.setValue("windowLayout/tiling/gap", 22);
    legacy.setValue("windowLayout/tiling/defaultWidth", 1100);
    legacy.setValue("windowLayout/tiling/splitTarget", "focused");
    legacy.sync();
    assert(legacy.status() == QSettings::NoError);
  }

  auto preferences = LunaDash::desktopPreferences();
  assert(preferences.at("themeMode") == "light");
  assert(preferences.at("workspaceCount") == 4);
  assert(preferences.at("startupApps").size() == 2);
  assert(preferences.at("wallpaperHistory").size() == 1);
  assert(preferences.at("wallpaperHistory").at(0) ==
         "/tmp/wallpaper-one.jpg");
  assert(preferences.at("wallpaperRevision") == 8);
  assert(LunaDash::setupComplete());
  const auto wallpaper = LunaDash::loadWallpaperSettings();
  assert(wallpaper.at("mode") == "shader");
  assert(wallpaper.at("image") == "/tmp/selected-wallpaper.png");
  assert(wallpaper.at("shader") == 1);
  assert(wallpaper.at("favorites").at(0) == "/tmp/favorite-wallpaper.jpg");
  const auto legacyLayout = LunaDash::loadWindowLayoutSettings(
      "tiling",
      {{"gap", {{"type", "integer"}}},
       {"defaultWidth", {{"type", "integer"}}},
       {"splitTarget", {{"type", "string"}}}},
      nullptr);
  assert(legacyLayout.at("gap") == 22);
  assert(legacyLayout.at("defaultWidth") == 1100);
  assert(legacyLayout.at("splitTarget") == "focused");

  std::string error;
  assert(LunaDash::updateDesktopPreferences({{"themeMode", "dark"}}, &error));
  {
    QSettings legacy;
    legacy.setValue("desktop/themeMode", "auto");
    legacy.setValue("appearance/wallpaperHistory",
                    QStringList{"/tmp/wallpaper-two.jpg"});
    legacy.sync();
  }
  preferences = LunaDash::desktopPreferences();
  assert(preferences.at("themeMode") == "dark");
  assert(preferences.at("wallpaperHistory").at(0) ==
         "/tmp/wallpaper-one.jpg");
  assert(!LunaDash::updateDesktopPreferences(
      {{"themeMode", "light"}, {"eyeCareTemperature", 0}}, &error));
  assert(LunaDash::desktopPreferences().at("themeMode") == "dark");
  assert(LunaDash::setSetupComplete(false, &error));
  assert(!LunaDash::setupComplete());

  const auto store = std::filesystem::path(temporary.path().toStdString()) /
                     "LunaDash/desktop-preferences.json";
  assert(std::filesystem::is_regular_file(store));
}
