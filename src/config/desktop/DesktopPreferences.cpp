#include "config/desktop/DesktopPreferences.hpp"

#include "config/desktop/DesktopPreferenceStore.hpp"

#include <gio/gio.h>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

Json parseLegacyList(std::string_view source);

bool decodeLegacyQtString(std::span<const unsigned char> bytes,
                          std::size_t &offset, std::string &result) {
  if (offset + 4 > bytes.size())
    return false;
  const std::uint32_t length =
      (static_cast<std::uint32_t>(bytes[offset]) << 24) |
      (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
      (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
      static_cast<std::uint32_t>(bytes[offset + 3]);
  offset += 4;
  if (length == 0xffffffffU) {
    result.clear();
    return true;
  }
  if (length > 4096 || (length & 1U) != 0 || offset + length > bytes.size())
    return false;

  std::vector<gunichar2> characters;
  characters.reserve(length / 2);
  for (std::size_t end = offset + length; offset < end; offset += 2)
    characters.push_back(static_cast<gunichar2>(
        (static_cast<unsigned int>(bytes[offset]) << 8) | bytes[offset + 1]));
  GError *error = nullptr;
  char *utf8 = g_utf16_to_utf8(characters.data(), characters.size(), nullptr,
                               nullptr, &error);
  if (error) {
    g_error_free(error);
    return false;
  }
  result = utf8 ? utf8 : "";
  g_free(utf8);
  return true;
}

std::optional<Json> legacyShortcutBindings(GKeyFile *file) {
  constexpr std::string_view prefix = "@Variant(";
  if (!file ||
      !g_key_file_has_key(file, "shortcuts", "bindings", nullptr))
    return std::nullopt;
  GError *keyError = nullptr;
  char *raw = g_key_file_get_value(file, "shortcuts", "bindings", &keyError);
  if (keyError) {
    g_error_free(keyError);
    return std::nullopt;
  }
  const std::string encoded = raw ? raw : "";
  g_free(raw);
  if (!std::string_view(encoded).starts_with(prefix) ||
      !std::string_view(encoded).ends_with(')'))
    return std::nullopt;

  std::vector<unsigned char> bytes;
  const auto payload = std::string_view(encoded).substr(
      prefix.size(), encoded.size() - prefix.size() - 1);
  bytes.reserve(payload.size());
  for (std::size_t index = 0; index < payload.size(); ++index) {
    if (payload[index] != '\\') {
      bytes.push_back(static_cast<unsigned char>(payload[index]));
      continue;
    }
    if (++index >= payload.size())
      return std::nullopt;
    if (payload[index] == '0') {
      unsigned int value = 0;
      std::size_t digits = 0;
      while (digits < 3 && index < payload.size() && payload[index] >= '0' &&
             payload[index] <= '7') {
        value = value * 8 + static_cast<unsigned int>(payload[index] - '0');
        ++digits;
        if (digits < 3 && index + 1 < payload.size() &&
            payload[index + 1] >= '0' && payload[index + 1] <= '7')
          ++index;
        else
          break;
      }
      bytes.push_back(static_cast<unsigned char>(value));
    } else if (payload[index] == 'x') {
      unsigned int value = 0;
      std::size_t digits = 0;
      while (digits < 2 && index + 1 < payload.size()) {
        const auto character = payload[index + 1];
        unsigned int digit = 0;
        if (character >= '0' && character <= '9')
          digit = static_cast<unsigned int>(character - '0');
        else if (character >= 'a' && character <= 'f')
          digit = static_cast<unsigned int>(character - 'a' + 10);
        else if (character >= 'A' && character <= 'F')
          digit = static_cast<unsigned int>(character - 'A' + 10);
        else
          break;
        value = value * 16 + digit;
        ++digits;
        ++index;
      }
      if (digits == 0)
        return std::nullopt;
      bytes.push_back(static_cast<unsigned char>(value));
    } else {
      switch (payload[index]) {
      case 'a':
        bytes.push_back('\a');
        break;
      case 'b':
        bytes.push_back('\b');
        break;
      case 'f':
        bytes.push_back('\f');
        break;
      case 'n':
        bytes.push_back('\n');
        break;
      case 'r':
        bytes.push_back('\r');
        break;
      case 't':
        bytes.push_back('\t');
        break;
      case 'v':
        bytes.push_back('\v');
        break;
      default:
        bytes.push_back(static_cast<unsigned char>(payload[index]));
        break;
      }
    }
    if (bytes.size() > 32768)
      return std::nullopt;
  }

  const auto readInteger = [&bytes](std::size_t &offset,
                                    std::uint32_t &value) {
    if (offset + 4 > bytes.size())
      return false;
    value = (static_cast<std::uint32_t>(bytes[offset]) << 24) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
            static_cast<std::uint32_t>(bytes[offset + 3]);
    offset += 4;
    return true;
  };
  std::size_t offset = 0;
  std::uint32_t type = 0;
  std::uint32_t count = 0;
  if (!readInteger(offset, type) || type != 8 ||
      !readInteger(offset, count) || count > 256)
    return std::nullopt;

  Json result = Json::object();
  for (std::uint32_t index = 0; index < count; ++index) {
    std::string key;
    std::string value;
    if (!decodeLegacyQtString(bytes, offset, key) ||
        !readInteger(offset, type) || type != 10 ||
        !decodeLegacyQtString(bytes, offset, value) || key.empty() ||
        key.size() > 80 || value.size() > 80)
      return std::nullopt;
    result[key] = value;
  }
  if (offset != bytes.size())
    return std::nullopt;
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

Json legacyWindowLayoutSettings(std::string_view templateKey,
                                const Json &schema) {
  if (templateKey.empty() || templateKey.size() > 128 ||
      templateKey.find_first_of("/\\") != std::string_view::npos ||
      templateKey.find('\0') != std::string_view::npos || !schema.is_object())
    return Json::object();
  const auto legacy = readLegacySettings();
  if (!legacy)
    return Json::object();

  constexpr std::string_view group = "windowLayout";
  Json settings = Json::object();
  for (auto it = schema.begin(); it != schema.end(); ++it) {
    const auto key = std::string(templateKey) + "\\" + it.key();
    if (!it.value().is_object() ||
        !g_key_file_has_key(legacy.get(), group.data(), key.c_str(), nullptr))
      continue;
    const auto type = it.value().value("type", std::string{});
    GError *error = nullptr;
    if (type == "boolean") {
      const auto value =
          g_key_file_get_boolean(legacy.get(), group.data(), key.c_str(),
                                 &error);
      if (!error)
        settings[it.key()] = static_cast<bool>(value);
    } else if (type == "integer") {
      const auto value =
          g_key_file_get_int64(legacy.get(), group.data(), key.c_str(),
                               &error);
      if (!error)
        settings[it.key()] = value;
    } else if (type == "number") {
      const auto value =
          g_key_file_get_double(legacy.get(), group.data(), key.c_str(),
                                &error);
      if (!error && std::isfinite(value))
        settings[it.key()] = value;
    } else if (type == "string") {
      auto value = keyString(legacy.get(), group.data(), key.c_str());
      if (value.size() <= 4096)
        settings[it.key()] = std::move(value);
    } else if (type == "array") {
      settings[it.key()] =
          parseLegacyList(keyString(legacy.get(), group.data(), key.c_str()));
    }
    if (error)
      g_error_free(error);
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

  if (!hasStoredShortcutBindings()) {
    if (const auto bindings = legacyShortcutBindings(file)) {
      std::string error;
      importLegacyShortcutBindings(*bindings, &error);
    }
  }
}

} // namespace

Json loadDisplayConfiguration(std::string_view output, std::string *error) {
  return loadLegacyDisplayConfiguration(output, error);
}

Json loadWindowLayoutSettings(std::string_view templateKey,
                              const Json &schema, std::string *error) {
  if (error)
    error->clear();
  if (hasStoredWindowLayoutSettings(templateKey))
    return LunaDash::loadWindowLayoutSettings(templateKey);

  auto settings = legacyWindowLayoutSettings(templateKey, schema);
  if (settings.empty())
    return settings;
  if (!saveWindowLayoutSettings(templateKey, settings, error))
    return Json::object();
  return settings;
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
