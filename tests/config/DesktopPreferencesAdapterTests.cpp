#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {
// Serialize a legacy settings map exactly as the old desktop stored it: a
// big-endian map type (8), entry count and UTF-16BE string keys and values.
void appendInteger(std::vector<unsigned char> &bytes, std::uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8)
    bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
}

void appendString(std::vector<unsigned char> &bytes, const std::string &text) {
  appendInteger(bytes, static_cast<std::uint32_t>(text.size() * 2));
  for (const char character : text) {
    bytes.push_back(0);
    bytes.push_back(static_cast<unsigned char>(character));
  }
}

std::string legacyStringMap(
    const std::vector<std::pair<std::string, std::string>> &entries) {
  std::vector<unsigned char> bytes;
  appendInteger(bytes, 8);
  appendInteger(bytes, static_cast<std::uint32_t>(entries.size()));
  for (const auto &[key, value] : entries) {
    appendString(bytes, key);
    appendInteger(bytes, 10);
    appendString(bytes, value);
  }
  static constexpr char digits[] = "0123456789abcdef";
  std::string encoded = "@Variant(";
  for (const auto byte : bytes) {
    encoded += "\\x";
    encoded.push_back(digits[byte >> 4]);
    encoded.push_back(digits[byte & 15]);
  }
  return encoded + ")";
}

void writeLegacySettings(const std::filesystem::path &path,
                         const std::string &contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  assert(output.good());
}
} // namespace

int main() {
  char pattern[] = "/tmp/lunadash-preferences-adapter-XXXXXX";
  const char *created = ::mkdtemp(pattern);
  assert(created);
  const std::filesystem::path temporary(created);
  ::setenv("XDG_CONFIG_HOME", temporary.c_str(), 1);
  const auto legacyPath = temporary / "LunaDash/LunaDash.conf";

  writeLegacySettings(
      legacyPath,
      "[appearance]\n"
      "wallpaper=1\n"
      "wallpaperFavorites=/tmp/favorite-wallpaper.jpg\n"
      "wallpaperHistory=/tmp/wallpaper-one.jpg\n"
      "wallpaperImage=/tmp/selected-wallpaper.png\n"
      "wallpaperMode=shader\n"
      "wallpaperRevision=8\n\n"
      "[desktop]\n"
      "startupApps=files, welcome\n"
      "themeMode=light\n"
      "workspaceCount=4\n\n"
      "[session]\n"
      "setupComplete=true\n\n"
      "[shortcuts]\n"
      "bindings=" +
          legacyStringMap({{"closeWindow", "Meta+Q"},
                           {"launchTerminal", "Meta+Return"}}) +
          "\n\n"
          "[windowLayout]\n"
          "tiling\\defaultWidth=1100\n"
          "tiling\\gap=22\n"
          "tiling\\splitTarget=focused\n");

  auto preferences = LunaDash::desktopPreferences();
  assert(preferences.at("themeMode") == "light");
  assert(preferences.at("workspaceCount") == 4);
  assert(preferences.at("startupApps").size() == 2);
  assert(preferences.at("wallpaperHistory").size() == 1);
  assert(preferences.at("wallpaperHistory").at(0) ==
         "/tmp/wallpaper-one.jpg");
  assert(preferences.at("wallpaperRevision") == 8);
  assert(LunaDash::setupComplete());
  assert(LunaDash::loadShortcutBindings() ==
         nlohmann::json({{"closeWindow", "Meta+Q"},
                         {"launchTerminal", "Meta+Return"}}));
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
  // Legacy files are imported once; later edits to them must be ignored.
  writeLegacySettings(legacyPath, "[appearance]\n"
                                  "wallpaperHistory=/tmp/wallpaper-two.jpg\n\n"
                                  "[desktop]\n"
                                  "themeMode=auto\n");
  preferences = LunaDash::desktopPreferences();
  assert(preferences.at("themeMode") == "dark");
  assert(preferences.at("wallpaperHistory").at(0) ==
         "/tmp/wallpaper-one.jpg");
  assert(!LunaDash::updateDesktopPreferences(
      {{"themeMode", "light"}, {"eyeCareTemperature", 0}}, &error));
  assert(LunaDash::desktopPreferences().at("themeMode") == "dark");
  assert(LunaDash::setSetupComplete(false, &error));
  assert(!LunaDash::setupComplete());

  const auto store = temporary / "LunaDash/desktop-preferences.json";
  assert(std::filesystem::is_regular_file(store));
  std::filesystem::remove_all(temporary);
}
