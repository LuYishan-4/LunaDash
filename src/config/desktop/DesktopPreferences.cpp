#include "config/desktop/DesktopPreferences.hpp"

#include "config/desktop/DesktopPreferenceStore.hpp"

#include <gio/gio.h>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace LunaDash {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

struct KeyFileDeleter {
  void operator()(GKeyFile *file) const {
    if (file)
      g_key_file_unref(file);
  }
};
using KeyFile = std::unique_ptr<GKeyFile, KeyFileDeleter>;

fs::path configRoot() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg &&
      fs::path(xdg).is_absolute())
    return xdg;
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / ".config";
  return {};
}

KeyFile readLegacySettings() {
  const auto root = configRoot();
  if (root.empty())
    return {};
  for (const auto *name : {"LunaDash/LunaDash.conf", "LunaDash/LunaDash.ini"}) {
    auto file = KeyFile(g_key_file_new());
    GError *error = nullptr;
    if (g_key_file_load_from_file(file.get(), (root / name).c_str(),
                                  G_KEY_FILE_NONE, &error))
      return file;
    if (error)
      g_error_free(error);
  }
  return {};
}

std::string keyString(GKeyFile *file, const char *group, const char *key) {
  if (!file || !g_key_file_has_key(file, group, key, nullptr))
    return {};
  GError *error = nullptr;
  char *value = g_key_file_get_string(file, group, key, &error);
  if (error) {
    g_error_free(error);
    return {};
  }
  std::string result = value ? value : "";
  g_free(value);
  return result;
}

Json loadLegacyDisplayConfiguration(std::string_view output,
                                    std::string *error) {
  if (error)
    error->clear();
  auto settings = loadDisplaySettings(output);
  if (!settings.empty())
    return settings;

  const auto legacy = readLegacySettings();
  if (!legacy || output.empty() || output.size() > 255 ||
      output.find('\0') != std::string_view::npos)
    return Json::object();

  const std::string group = "display/" + std::string(output);
  for (const auto *key : {"width", "height", "refresh"}) {
    if (!g_key_file_has_key(legacy.get(), group.c_str(), key, nullptr))
      continue;
    GError *readError = nullptr;
    const int value =
        g_key_file_get_integer(legacy.get(), group.c_str(), key, &readError);
    if (readError) {
      g_error_free(readError);
      continue;
    }
    settings[key] = value;
  }
  if (g_key_file_has_key(legacy.get(), group.c_str(), "scale", nullptr)) {
    GError *readError = nullptr;
    const double value =
        g_key_file_get_double(legacy.get(), group.c_str(), "scale", &readError);
    if (readError) {
      g_error_free(readError);
    } else {
      settings["scale"] = value;
    }
  }
  if (settings.empty())
    return settings;

  std::string saveError;
  if (!saveDisplaySettings(output, settings, &saveError)) {
    if (error)
      *error = std::move(saveError);
    return Json::object();
  }
  return settings;
}

Json parseLegacyList(std::string_view source) {
  if (source.starts_with("@StringList(") && source.ends_with(')')) {
    source.remove_prefix(std::string_view("@StringList(").size());
    source.remove_suffix(1);
  } else if (source == "@Invalid()" || source.starts_with("@Variant(")) {
    return Json::array();
  }

  Json result = Json::array();
  std::string item;
  bool quoted = false;
  bool escaped = false;
  bool hadQuote = false;
  for (const char character : source) {
    if (escaped) {
      switch (character) {
      case 'n':
        item.push_back('\n');
        break;
      case 'r':
        item.push_back('\r');
        break;
      case 't':
        item.push_back('\t');
        break;
      default:
        item.push_back(character);
        break;
      }
      escaped = false;
    } else if (character == '\\' && quoted) {
      escaped = true;
    } else if (character == '"') {
      if (!quoted && item.find_first_not_of(" \t") == std::string::npos)
        item.clear();
      quoted = !quoted;
      hadQuote = true;
    } else if (character == ',' && !quoted) {
      if (!hadQuote) {
        const auto first = item.find_first_not_of(" \t");
        const auto last = item.find_last_not_of(" \t");
        item = first == std::string::npos ? "" : item.substr(first, last - first + 1);
      }
      result.push_back(item);
      item.clear();
      hadQuote = false;
    } else {
      item.push_back(character);
    }
  }
  if (escaped)
    item.push_back('\\');
  if (!hadQuote) {
    const auto first = item.find_first_not_of(" \t");
    const auto last = item.find_last_not_of(" \t");
    item = first == std::string::npos ? "" : item.substr(first, last - first + 1);
  }
  if (!source.empty() || !result.empty())
    result.push_back(std::move(item));
  return result;
}

Json parseLegacyValue(GKeyFile *file, const std::string &key,
                      const Json &defaultValue) {
  if (!file)
    return nullptr;
  const auto raw = keyString(file, "desktop", key.c_str());
  if (raw.empty() && !g_key_file_has_key(file, "desktop", key.c_str(), nullptr))
    return nullptr;
  if (defaultValue.is_boolean()) {
    if (raw == "true" || raw == "1")
      return true;
    if (raw == "false" || raw == "0")
      return false;
    return nullptr;
  }
  if (defaultValue.is_number_integer()) {
    std::int64_t value = 0;
    const auto [end, error] =
        std::from_chars(raw.data(), raw.data() + raw.size(), value);
    return error == std::errc{} && end == raw.data() + raw.size()
               ? Json(value)
               : Json(nullptr);
  }
  if (defaultValue.is_number_float()) {
    char *end = nullptr;
    const double value = std::strtod(raw.c_str(), &end);
    return end == raw.c_str() + raw.size() && end != raw.c_str() &&
                   std::isfinite(value)
               ? Json(value)
               : Json(nullptr);
  }
  if (defaultValue.is_array())
    return parseLegacyList(raw);
  if (defaultValue.is_string())
    return raw;
  return nullptr;
}

void importLegacySettings(GKeyFile *file) {
  if (!hasStoredDesktopPreferences()) {
    Json values = Json::object();
    const auto defaults = defaultDesktopPreferenceValues();
    for (auto it = defaults.begin(); it != defaults.end(); ++it) {
      const auto value = parseLegacyValue(file, it.key(), it.value());
      if (!value.is_null())
        values[it.key()] = value;
    }
    GError *ignored = nullptr;
    const gboolean setup = file &&
        g_key_file_get_boolean(file, "session", "setupComplete", &ignored);
    if (ignored)
      g_error_free(ignored);
    std::string error;
    importLegacyDesktopPreferences(values, setup, &error);
  }

  if (!hasStoredWallpaperSettings()) {
    const auto list = [file](const char *group, const char *key) {
      return parseLegacyList(keyString(file, group, key));
    };
    Json history = list("appearance", "wallpaperHistory");
    if (history.empty())
      history = list("desktop", "wallpaperHistory");
    const auto mode = keyString(file, "appearance", "wallpaperMode");
    const auto image = keyString(file, "appearance", "wallpaperImage");
    const auto refresh = keyString(file, "appearance", "wallpaperLibraryRefresh");
    const auto parseInteger = [file](const char *group, const char *key) {
      const auto raw = keyString(file, group, key);
      std::uint64_t value = 0;
      const auto [end, error] =
          std::from_chars(raw.data(), raw.data() + raw.size(), value);
      return error == std::errc{} && end == raw.data() + raw.size() ? value : 0;
    };
    Json legacy{
        {"mode", mode.empty() ? "image" : mode},
        {"image", image},
        {"shader", parseInteger("appearance", "wallpaper")},
        {"history", std::move(history)},
        {"favorites", list("appearance", "wallpaperFavorites")},
        {"revision", parseInteger("appearance", "wallpaperRevision")},
        {"libraryRefresh", refresh}};
    std::string error;
    importLegacyWallpaperSettings(legacy, &error);
  }
}

} // namespace

Json loadDisplayConfiguration(std::string_view output, std::string *error) {
  return loadLegacyDisplayConfiguration(output, error);
}

void ensureLegacyDesktopSettingsImported() {
  const auto settings = readLegacySettings();
  importLegacySettings(settings.get());
}

Json desktopPreferences() {
  ensureLegacyDesktopSettingsImported();
  auto result = loadDesktopPreferences();
  const auto wallpaper = loadWallpaperSettings();
  result["wallpaperHistory"] =
      wallpaper.value("history", Json::array());
  result["wallpaperRevision"] =
      wallpaper.value("revision", std::uint64_t{0});
  return result;
}

bool updateDesktopPreferences(const Json &changes, std::string *error) {
  ensureLegacyDesktopSettingsImported();
  return saveDesktopPreferences(changes, error);
}

bool setupComplete() {
  ensureLegacyDesktopSettingsImported();
  return isDesktopSetupComplete();
}

bool setSetupComplete(bool complete, std::string *error) {
  ensureLegacyDesktopSettingsImported();
  return setDesktopSetupComplete(complete, error);
}

} // namespace LunaDash
