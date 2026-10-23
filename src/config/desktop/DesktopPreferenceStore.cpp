#include "config/desktop/DesktopPreferenceStore.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string_view>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

fs::path configRoot() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME");
      xdg && *xdg && fs::path(xdg).is_absolute())
    return xdg;
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / ".config";
  return {};
}

fs::path picturesDirectory() {
  const char *homeValue = std::getenv("HOME");
  if (!homeValue || !*homeValue)
    return {};
  const fs::path home(homeValue);
  std::ifstream input(configRoot() / "user-dirs.dirs");
  std::string line;
  while (std::getline(input, line)) {
    constexpr std::string_view key = "XDG_PICTURES_DIR=";
    if (!std::string_view(line).starts_with(key))
      continue;
    auto value = std::string_view(line).substr(key.size());
    if (value.size() < 2 || value.front() != '"' || value.back() != '"')
      break;
    value.remove_prefix(1);
    value.remove_suffix(1);
    std::string expanded;
    for (std::size_t index = 0; index < value.size(); ++index) {
      if (value[index] == '\\' && index + 1 < value.size()) {
        expanded.push_back(value[++index]);
      } else if (value.substr(index).starts_with("$HOME")) {
        expanded += home.string();
        index += 4;
      } else if (value.substr(index).starts_with("${HOME}")) {
        expanded += home.string();
        index += 6;
      } else {
        expanded.push_back(value[index]);
      }
    }
    if (fs::path(expanded).is_absolute())
      return expanded;
    break;
  }
  return home / "Pictures";
}

fs::path preferencesFilePath() {
  const auto root = configRoot();
  if (root.empty())
    return {};
  return root / "LunaDash/desktop-preferences.json";
}

void setError(std::string *error, std::string message);

class StoreLock final {
public:
  explicit StoreLock(std::string *error) {
    const auto path = preferencesFilePath();
    if (path.empty()) {
      setError(error, "Could not locate the XDG configuration directory.");
      return;
    }
    std::error_code filesystemError;
    fs::create_directories(path.parent_path(), filesystemError);
    if (filesystemError) {
      setError(error, "Could not create the desktop preferences directory.");
      return;
    }
    const auto lockPath = path.string() + ".lock";
    descriptor_ = ::open(lockPath.c_str(),
                         O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor_ < 0) {
      setError(error, "Could not open the desktop preferences lock.");
      return;
    }
    while (::flock(descriptor_, LOCK_EX) != 0) {
      if (errno == EINTR)
        continue;
      setError(error, "Could not lock desktop preferences.");
      ::close(descriptor_);
      descriptor_ = -1;
      return;
    }
  }

  ~StoreLock() {
    if (descriptor_ >= 0) {
      ::flock(descriptor_, LOCK_UN);
      ::close(descriptor_);
    }
  }

  StoreLock(const StoreLock &) = delete;
  StoreLock &operator=(const StoreLock &) = delete;

  explicit operator bool() const { return descriptor_ >= 0; }

private:
  int descriptor_ = -1;
};

Json emptyStore() {
  return {{"schema", 1},
          {"preferences", Json::object()},
          {"setupComplete", false},
          {"desktopPreferencesImported", false},
          {"legacyLanguagePreferenceIgnored", false},
          {"displays", Json::object()}};
}

Json readStore() {
  auto result = emptyStore();
  const auto path = preferencesFilePath();
  if (path.empty())
    return result;
  std::error_code error;
  const auto size = fs::file_size(path, error);
  if (error || size > 1024 * 1024)
    return result;
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return result;
  try {
    auto parsed = Json::parse(input);
    if (!parsed.is_object() || parsed.value("schema", 0) != 1)
      return result;
    const auto preferences = parsed.find("preferences");
    if (preferences != parsed.end() && preferences->is_object())
      result["preferences"] = *preferences;
    const auto setupComplete = parsed.find("setupComplete");
    if (setupComplete != parsed.end() && setupComplete->is_boolean())
      result["setupComplete"] = *setupComplete;
    const auto preferencesImported = parsed.find("desktopPreferencesImported");
    if (preferencesImported != parsed.end() &&
        preferencesImported->is_boolean())
      result["desktopPreferencesImported"] = *preferencesImported;
    const auto legacyLanguageIgnored =
        parsed.find("legacyLanguagePreferenceIgnored");
    if (legacyLanguageIgnored != parsed.end() &&
        legacyLanguageIgnored->is_boolean())
      result["legacyLanguagePreferenceIgnored"] = *legacyLanguageIgnored;
    const auto wallpaper = parsed.find("wallpaper");
    if (wallpaper != parsed.end() && wallpaper->is_object())
      result["wallpaper"] = *wallpaper;
    const auto shortcuts = parsed.find("shortcuts");
    if (shortcuts != parsed.end() && shortcuts->is_object())
      result["shortcuts"] = *shortcuts;
    const auto displays = parsed.find("displays");
    if (displays != parsed.end() && displays->is_object())
      result["displays"] = *displays;
  } catch (const Json::exception &) {
  }
  return result;
}

void setError(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
}

bool writeAll(int fd, std::string_view bytes) {
  while (!bytes.empty()) {
    const auto written = ::write(fd, bytes.data(), bytes.size());
    if (written < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    if (written == 0)
      return false;
    bytes.remove_prefix(static_cast<std::size_t>(written));
  }
  return true;
}

bool writeStore(const Json &store, std::string *error) {
  const auto path = preferencesFilePath();
  if (path.empty()) {
    setError(error, "Could not locate the XDG configuration directory.");
    return false;
  }
  std::error_code filesystemError;
  fs::create_directories(path.parent_path(), filesystemError);
  if (filesystemError) {
    setError(error, "Could not create the desktop preferences directory.");
    return false;
  }

  static std::atomic_uint64_t nextTemporary{0};
  const auto temporary = path.string() + ".tmp." + std::to_string(::getpid()) +
                         "." + std::to_string(nextTemporary.fetch_add(1));
  const int fd =
      ::open(temporary.c_str(),
             O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (fd < 0) {
    setError(error, "Could not create a temporary preferences file.");
    return false;
  }
  const auto bytes = store.dump(2) + "\n";
  bool success = writeAll(fd, bytes) && ::fsync(fd) == 0;
  const int closeResult = ::close(fd);
  success = success && closeResult == 0;
  if (success)
    success = ::rename(temporary.c_str(), path.c_str()) == 0;
  if (!success) {
    ::unlink(temporary.c_str());
    setError(error, "Could not atomically save desktop preferences.");
    return false;
  }

  const int directoryFd =
      ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (directoryFd >= 0) {
    ::fsync(directoryFd);
    ::close(directoryFd);
  }
  return true;
}

bool isString(const Json &value, std::size_t maximumBytes) {
  if (!value.is_string())
    return false;
  const auto &text = value.get_ref<const std::string &>();
  return text.size() <= maximumBytes && text.find('\0') == std::string::npos;
}

bool withinUtf16Length(std::string_view text, std::size_t maximum) {
  std::size_t units = 0;
  for (std::size_t index = 0; index < text.size();) {
    const auto lead = static_cast<unsigned char>(text[index]);
    std::size_t length = lead < 0x80             ? 1
                         : (lead & 0xe0) == 0xc0 ? 2
                         : (lead & 0xf0) == 0xe0 ? 3
                         : (lead & 0xf8) == 0xf0 ? 4
                                                 : 0;
    if (length == 0 || index + length > text.size())
      return false;
    for (std::size_t offset = 1; offset < length; ++offset)
      if ((static_cast<unsigned char>(text[index + offset]) & 0xc0) != 0x80)
        return false;
    const std::size_t added = length == 4 ? 2 : 1;
    if (added > maximum - std::min(units, maximum))
      return false;
    units += added;
    index += length;
  }
  return units <= maximum;
}

bool validPort(std::string_view text) {
  if (text.empty())
    return false;
  unsigned int port = 0;
  const auto [parsed, error] =
      std::from_chars(text.data(), text.data() + text.size(), port);
  return error == std::errc{} && parsed == text.data() + text.size() &&
         port <= 65535;
}

bool validHostPort(std::string_view hostPort) {
  if (hostPort.empty())
    return false;
  if (hostPort.front() == '[') {
    const auto close = hostPort.find(']');
    if (close == std::string_view::npos || close <= 1)
      return false;
    const auto suffix = hostPort.substr(close + 1);
    return suffix.empty() ||
           (suffix.front() == ':' && validPort(suffix.substr(1)));
  }
  const auto firstColon = hostPort.find(':');
  if (firstColon == std::string_view::npos)
    return true;
  if (hostPort.find(':', firstColon + 1) != std::string_view::npos ||
      firstColon == 0)
    return false;
  return validPort(hostPort.substr(firstColon + 1));
}

bool validColor(const Json &value) {
  if (!isString(value, 7))
    return false;
  const auto &color = value.get_ref<const std::string &>();
  if (color.size() != 7 || color.front() != '#')
    return false;
  return std::all_of(color.begin() + 1, color.end(), [](unsigned char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
  });
}

bool validProxyText(const Json &value, bool url) {
  if (!isString(value, 512))
    return false;
  const auto &text = value.get_ref<const std::string &>();
  if (text.find_first_of("\r\n\t ") != std::string::npos)
    return false;
  if (!url || text.empty())
    return true;
  const auto separator = text.find("://");
  if (separator == std::string::npos)
    return false;
  auto scheme = text.substr(0, separator);
  std::transform(
      scheme.begin(), scheme.end(), scheme.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (scheme != "http" && scheme != "https" && scheme != "socks" &&
      scheme != "socks5")
    return false;
  const auto authorityStart = separator + 3;
  const auto authorityEnd = text.find_first_of("/?#", authorityStart);
  const auto authority =
      text.substr(authorityStart, authorityEnd == std::string::npos
                                      ? std::string::npos
                                      : authorityEnd - authorityStart);
  if (authority.empty())
    return false;
  const auto at = authority.rfind('@');
  const auto hostPort = authority.substr(at == std::string::npos ? 0 : at + 1);
  return hostPort.find_first_of("<>\"'") == std::string::npos &&
         validHostPort(hostPort);
}

bool validDesktopPreference(std::string_view key, const Json &value) {
  if (key == "weatherEnabled" || key == "syncFcitxTheme")
    return value.is_boolean();
  if (key == "weatherLocation")
    return isString(value, 320) &&
           withinUtf16Length(value.get_ref<const std::string &>(), 80);
  if (key == "weatherLatitude" || key == "weatherLongitude") {
    if (!value.is_number())
      return false;
    const double number = value.get<double>();
    const double limit = key == "weatherLatitude" ? 90.0 : 180.0;
    return std::isfinite(number) && number >= -limit && number <= limit;
  }
  if (key == "language")
    return value == "en_US" || value == "zh_TW" || value == "zh_CN" ||
           value == "ja_JP";
  if (key == "themeMode")
    return value.is_string() &&
           (value == "dark" || value == "light" || value == "auto");
  if (key == "wallpaperDirectory")
    return isString(value, 4096) &&
           fs::path(value.get<std::string>()).is_absolute() &&
           value.get_ref<const std::string &>().find('\n') == std::string::npos;
  if (key == "wallpaperColors" || key == "eyeCare" ||
      key == "syncApplicationThemes" || key == "dockEnabled" ||
      key == "dockAutoHide")
    return value.is_boolean();
  if (key == "keyboardLayout")
    return value.is_string() &&
           (value == "us" || value == "gb" || value == "de" || value == "fr" ||
            value == "es" || value == "jp" || value == "tw");
  if (key == "fontFamily")
    return value.is_string() &&
           (value == "sans-serif" || value == "serif" || value == "monospace");
  if (key == "updateChannel")
    return value == "stable" || value == "dev";
  if (key == "proxyHttp" || key == "proxyHttps" || key == "proxySocks")
    return validProxyText(value, true);
  if (key == "proxyBypass")
    return validProxyText(value, false);
  if (key == "startupApps") {
    if (!value.is_array() || value.size() > 3)
      return false;
    std::vector<std::string> seen;
    for (const auto &app : value) {
      if (!isString(app, 32))
        return false;
      const auto name = app.get<std::string>();
      if ((name != "files" && name != "packages" && name != "welcome") ||
          std::find(seen.begin(), seen.end(), name) != seen.end())
        return false;
      seen.push_back(name);
    }
    return true;
  }
  if (key == "colorPins") {
    if (!value.is_array() || value.size() > 16)
      return false;
    std::vector<std::string> seen;
    for (const auto &color : value) {
      if (!validColor(color))
        return false;
      auto normalized = color.get<std::string>();
      std::transform(
          normalized.begin(), normalized.end(), normalized.begin(),
          [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
      if (std::find(seen.begin(), seen.end(), normalized) != seen.end())
        return false;
      seen.push_back(std::move(normalized));
    }
    return true;
  }
  if (key == "workspaceCount" || key == "eyeCareTemperature" ||
      key == "cursorSize") {
    if (!value.is_number())
      return false;
    const double number = value.get<double>();
    const double minimum = key == "workspaceCount"       ? 1
                           : key == "eyeCareTemperature" ? 2500
                                                         : 16;
    const double maximum = key == "workspaceCount"       ? 10
                           : key == "eyeCareTemperature" ? 6500
                                                         : 64;
    return std::isfinite(number) && std::floor(number) == number &&
           number >= minimum && number <= maximum;
  }
  if (key == "accent" || key == "secondaryAccent")
    return validColor(value);
  if (key == "overview" || key == "showHostDetails" || key == "blur" ||
      key == "animations" || key == "clock24Hour" || key == "proxyEnabled")
    return value.is_boolean();
  if (key == "gap" || key == "panelHeight" || key == "blurRadius" ||
      key == "windowOpacity" || key == "animationDuration") {
    if (!value.is_number())
      return false;
    const double number = value.get<double>();
    const double minimum = key == "gap"             ? 4
                           : key == "panelHeight"   ? 32
                           : key == "windowOpacity" ? 60
                                                    : 0;
    const double maximum = key == "gap" || key == "blurRadius" ? 32
                           : key == "panelHeight"              ? 56
                           : key == "windowOpacity"            ? 100
                                                               : 600;
    return std::isfinite(number) && std::floor(number) == number &&
           number >= minimum && number <= maximum;
  }
  return false;
}

Json defaultWallpaperSettings() {
  return {{"mode", "image"},
          {"image", ""},
          {"shader", 0},
          {"history", Json::array()},
          {"favorites", Json::array()},
          {"revision", std::uint64_t{0}},
          {"libraryRefresh", ""}};
}

Json desktopPreferencesFromStore(const Json &store) {
  auto result = defaultDesktopPreferenceValues();
  const auto preferences = store.find("preferences");
  if (preferences == store.end() || !preferences->is_object())
    return result;
  for (auto it = preferences->begin(); it != preferences->end(); ++it)
    if (validDesktopPreference(it.key(), it.value()))
      result[it.key()] = it.value();
  return result;
}

bool validWallpaperSetting(std::string_view key, const Json &value) {
  if (key == "mode")
    return value == "image" || value == "shader";
  if (key == "image")
    return isString(value, 4096) &&
           (value.get_ref<const std::string &>().empty() ||
            fs::path(value.get<std::string>()).is_absolute());
  if (key == "shader")
    return value.is_number() &&
           (value.get<double>() == 0 || value.get<double>() == 1);
  if (key == "history" || key == "favorites") {
    const std::size_t maximum = key == "history" ? 12 : 256;
    if (!value.is_array() || value.size() > maximum)
      return false;
    std::vector<std::string> seen;
    for (const auto &entry : value) {
      if (!isString(entry, 4096) ||
          !fs::path(entry.get<std::string>()).is_absolute())
        return false;
      const auto path = entry.get<std::string>();
      if (std::find(seen.begin(), seen.end(), path) != seen.end())
        return false;
      seen.push_back(path);
    }
    return true;
  }
  if (key == "revision")
    return value.is_number_unsigned() ||
           (value.is_number_integer() && value.get<std::int64_t>() >= 0);
  if (key == "libraryRefresh")
    return isString(value, 64);
  return false;
}

Json wallpaperSettingsFromStore(const Json &store) {
  auto result = defaultWallpaperSettings();
  const auto saved = store.find("wallpaper");
  if (saved == store.end() || !saved->is_object())
    return result;
  for (auto it = saved->begin(); it != saved->end(); ++it)
    if (validWallpaperSetting(it.key(), it.value()))
      result[it.key()] = it.value();
  return result;
}

bool integerWithin(const Json &value, std::int64_t minimum,
                   std::int64_t maximum) {
  if (!value.is_number_integer())
    return false;
  if (value.is_number_unsigned()) {
    const auto number = value.get<std::uint64_t>();
    return maximum >= 0 &&
           number >= static_cast<std::uint64_t>(std::max<std::int64_t>(0, minimum)) &&
           number <= static_cast<std::uint64_t>(maximum);
  }
  const auto number = value.get<std::int64_t>();
  return number >= minimum && number <= maximum;
}

} // namespace

Json defaultDesktopPreferenceValues() {
  return {
      {"accent", "#9ccbfb"},
      {"secondaryAccent", "#41576b"},
      {"themeMode", "dark"},
      {"wallpaperColors", true},
      {"wallpaperDirectory", (picturesDirectory() / "Wallpapers").string()},
      {"eyeCare", false},
      {"eyeCareTemperature", 4500},
      {"syncApplicationThemes", false},
      {"syncFcitxTheme", false},
      {"weatherEnabled", false},
      {"weatherLocation", ""},
      {"weatherLatitude", 0.0},
      {"weatherLongitude", 0.0},
      {"dockEnabled", false},
      {"dockAutoHide", true},
      {"colorPins", {"#9ccbfb", "#c4b5fd", "#7dcccf", "#e7b899", "#41576b"}},
      {"gap", 12},
      {"panelHeight", 40},
      {"blur", true},
      {"blurRadius", 18},
      {"windowOpacity", 90},
      {"animations", true},
      {"animationDuration", 220},
      {"workspaceCount", 10},
      {"keyboardLayout", "us"},
      {"cursorSize", 24},
      {"fontFamily", "sans-serif"},
      {"clock24Hour", true},
      {"startupApps", Json::array()},
      {"proxyEnabled", false},
      {"proxyHttp", ""},
      {"proxyHttps", ""},
      {"proxySocks", ""},
      {"proxyBypass", ""},
      {"overview", false},
      {"showHostDetails", false},
      {"updateChannel", "stable"}};
}

Json loadDesktopPreferences() {
  return desktopPreferencesFromStore(readStore());
}

bool saveDesktopPreferences(const Json &changes, std::string *error) {
  if (error)
    error->clear();
  if (!changes.is_object()) {
    setError(error, "Desktop preference changes must be a JSON object.");
    return false;
  }
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if (!validDesktopPreference(it.key(), it.value())) {
      setError(error, "Invalid desktop preference: " + it.key());
      return false;
    }
  }
  StoreLock lock(error);
  if (!lock)
    return false;
  auto store = readStore();
  auto preferences = desktopPreferencesFromStore(store);
  for (auto it = changes.begin(); it != changes.end(); ++it)
    preferences[it.key()] = it.value();
  store["preferences"] = std::move(preferences);
  store["desktopPreferencesImported"] = true;
  return writeStore(store, error);
}

bool resetDesktopPreferences(std::string *error) {
  if (error)
    error->clear();
  StoreLock lock(error);
  if (!lock)
    return false;
  auto store = readStore();
  store["preferences"] = defaultDesktopPreferenceValues();
  store["desktopPreferencesImported"] = true;
  store["legacyLanguagePreferenceIgnored"] = true;
  return writeStore(store, error);
}

bool hasStoredDesktopPreferences() {
  const auto path = preferencesFilePath();
  if (path.empty())
    return false;
  std::error_code error;
  if (!fs::is_regular_file(path, error) || error)
    return false;
  const auto size = fs::file_size(path, error);
  if (error || size > 1024 * 1024)
    return false;
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return false;
  try {
    const auto store = Json::parse(input);
    if (!store.is_object() || store.value("schema", 0) != 1 ||
        !store.contains("preferences") || !store.at("preferences").is_object())
      return false;
    return store.value("desktopPreferencesImported", false) ||
           !store.at("preferences").empty();
  } catch (const Json::exception &) {
    return false;
  }
}

bool legacyLanguagePreferenceIgnored() {
  return readStore().value("legacyLanguagePreferenceIgnored", false);
}

bool importLegacyDesktopPreferences(const Json &values, bool setupComplete,
                                    std::string *error) {
  if (error)
    error->clear();
  StoreLock lock(error);
  if (!lock)
    return false;
  if (hasStoredDesktopPreferences())
    return true;
  auto store = readStore();
  auto preferences = defaultDesktopPreferenceValues();
  if (values.is_object()) {
    for (auto it = values.begin(); it != values.end(); ++it) {
      if (validDesktopPreference(it.key(), it.value()))
        preferences[it.key()] = it.value();
    }
  }
  store["preferences"] = std::move(preferences);
  store["setupComplete"] = setupComplete;
  store["desktopPreferencesImported"] = true;
  return writeStore(store, error);
}

Json loadWallpaperSettings() { return wallpaperSettingsFromStore(readStore()); }

bool saveWallpaperSettings(const Json &changes, std::string *error) {
  if (error)
    error->clear();
  if (!changes.is_object()) {
    setError(error, "Wallpaper settings changes must be a JSON object.");
    return false;
  }
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if (!validWallpaperSetting(it.key(), it.value())) {
      setError(error, "Invalid wallpaper setting: " + it.key());
      return false;
    }
  }
  StoreLock lock(error);
  if (!lock)
    return false;
  auto store = readStore();
  auto wallpaper = wallpaperSettingsFromStore(store);
  for (auto it = changes.begin(); it != changes.end(); ++it)
    wallpaper[it.key()] = it.value();
  store["wallpaper"] = std::move(wallpaper);
  return writeStore(store, error);
}

bool hasStoredWallpaperSettings() {
  const auto store = readStore();
  return store.contains("wallpaper") && store.at("wallpaper").is_object();
}

bool importLegacyWallpaperSettings(const Json &values, std::string *error) {
  if (error)
    error->clear();
  StoreLock lock(error);
  if (!lock)
    return false;
  if (hasStoredWallpaperSettings())
    return true;
  auto wallpaper = defaultWallpaperSettings();
  if (values.is_object()) {
    for (auto it = values.begin(); it != values.end(); ++it) {
      if ((it.key() == "history" || it.key() == "favorites") &&
          it.value().is_array()) {
        const std::size_t maximum = it.key() == "history" ? 12 : 256;
        Json normalized = Json::array();
        std::vector<std::string> seen;
        for (const auto &entry : it.value()) {
          if (!isString(entry, 4096) ||
              !fs::path(entry.get<std::string>()).is_absolute())
            continue;
          const auto path = entry.get<std::string>();
          if (std::find(seen.begin(), seen.end(), path) != seen.end())
            continue;
          seen.push_back(path);
          normalized.push_back(path);
          if (normalized.size() >= maximum)
            break;
        }
        wallpaper[it.key()] = std::move(normalized);
      } else if (validWallpaperSetting(it.key(), it.value())) {
        wallpaper[it.key()] = it.value();
      }
    }
  }
  auto store = readStore();
  store["wallpaper"] = std::move(wallpaper);
  return writeStore(store, error);
}

Json loadShortcutBindings() {
  const auto store = readStore();
  const auto bindings = store.find("shortcuts");
  if (bindings == store.end() || !bindings->is_object())
    return Json::object();
  return *bindings;
}

bool saveShortcutBindings(const Json &bindings, std::string *error) {
  if (error)
    error->clear();
  if (!bindings.is_object()) {
    setError(error, "Shortcut bindings must be a JSON object.");
    return false;
  }
  for (auto it = bindings.begin(); it != bindings.end(); ++it) {
    if (!isString(it.value(), 80) || it.key().empty() || it.key().size() > 80 ||
        it.key().find('\0') != std::string::npos) {
      setError(error, "Invalid shortcut binding: " + it.key());
      return false;
    }
  }
  StoreLock lock(error);
  if (!lock)
    return false;
  auto store = readStore();
  store["shortcuts"] = bindings;
  return writeStore(store, error);
}

bool hasStoredShortcutBindings() {
  const auto store = readStore();
  return store.contains("shortcuts") && store.at("shortcuts").is_object();
}

bool importLegacyShortcutBindings(const Json &bindings, std::string *error) {
  if (error)
    error->clear();
  StoreLock lock(error);
  if (!lock)
    return false;
  if (hasStoredShortcutBindings())
    return true;
  Json imported = Json::object();
  if (bindings.is_object()) {
    for (auto it = bindings.begin(); it != bindings.end(); ++it)
      if (!it.key().empty() && it.key().size() <= 80 &&
          it.key().find('\0') == std::string::npos && isString(it.value(), 80))
        imported[it.key()] = it.value();
  }
  auto store = readStore();
  store["shortcuts"] = std::move(imported);
  return writeStore(store, error);
}

Json loadDisplaySettings(std::string_view output) {
  if (output.empty() || output.size() > 255 ||
      output.find('\0') != std::string_view::npos)
    return Json::object();
  const auto store = readStore();
  const auto displays = store.find("displays");
  if (displays == store.end() || !displays->is_object())
    return Json::object();
  const auto settings = displays->find(std::string(output));
  if (settings == displays->end() || !settings->is_object())
    return Json::object();

  Json valid = Json::object();
  for (auto it = settings->begin(); it != settings->end(); ++it) {
    if (it.key() == "width" || it.key() == "height" ||
        it.key() == "refresh") {
      const auto minimum = it.key() == "refresh" ? 0 : 1;
      const auto maximum = it.key() == "refresh" ? 1000000 : 32768;
      if (integerWithin(it.value(), minimum, maximum))
        valid[it.key()] = it.value();
    } else if (it.key() == "scale" && it.value().is_number()) {
      const auto value = it.value().get<double>();
      if (std::isfinite(value) && value >= 1.0 && value <= 3.0)
        valid[it.key()] = value;
    }
  }
  return valid;
}

bool saveDisplaySettings(std::string_view output, const Json &settings,
                         std::string *error) {
  if (error)
    error->clear();
  if (output.empty() || output.size() > 255 ||
      output.find('\0') != std::string_view::npos ||
      !settings.is_object() || settings.empty()) {
    setError(error, "Display settings have an invalid output or value.");
    return false;
  }
  for (auto it = settings.begin(); it != settings.end(); ++it) {
    if (it.key() == "width" || it.key() == "height") {
      if (!integerWithin(it.value(), 1, 32768)) {
        setError(error, "Display dimensions must be between 1 and 32768.");
        return false;
      }
    } else if (it.key() == "refresh") {
      if (!integerWithin(it.value(), 0, 1000000)) {
        setError(error, "Display refresh rate is invalid.");
        return false;
      }
    } else if (it.key() == "scale") {
      if (!it.value().is_number() ||
          !std::isfinite(it.value().get<double>()) ||
          it.value().get<double>() < 1.0 ||
          it.value().get<double>() > 3.0) {
        setError(error, "Display scale must be between 1 and 3.");
        return false;
      }
    } else {
      setError(error, "Unknown display setting: " + it.key());
      return false;
    }
  }

  StoreLock lock(error);
  if (!lock)
    return false;
  auto store = readStore();
  auto &displays = store["displays"];
  if (!displays.is_object())
    displays = Json::object();
  const std::string outputName(output);
  if (!displays.contains(outputName) && displays.size() >= 64) {
    setError(error, "Too many saved display configurations.");
    return false;
  }
  auto saved = displays.value(outputName, Json::object());
  if (!saved.is_object())
    saved = Json::object();
  for (auto it = settings.begin(); it != settings.end(); ++it)
    saved[it.key()] = it.value();
  displays[outputName] = std::move(saved);
  return writeStore(store, error);
}

bool isDesktopSetupComplete() {
  if (const char *skip = std::getenv("LUDASH_SKIP_SETUP");
      skip && std::string_view(skip) == "1")
    return true;
  const auto store = readStore();
  return store.value("setupComplete", false);
}

bool setDesktopSetupComplete(bool complete, std::string *error) {
  if (error)
    error->clear();
  StoreLock lock(error);
  if (!lock)
    return false;
  auto store = readStore();
  store["setupComplete"] = complete;
  store["desktopPreferencesImported"] = true;
  return writeStore(store, error);
}

} // namespace LunaDash
