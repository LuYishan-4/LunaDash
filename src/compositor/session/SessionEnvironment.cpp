#include "compositor/session/SessionEnvironment.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

extern char **environ;

namespace LunaDash {
namespace {
namespace fs = std::filesystem;

constexpr const char *kProxyVariables[] = {
    "http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY",
    "all_proxy",  "ALL_PROXY",  "no_proxy",    "NO_PROXY"};

std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](char c) {
    return std::isspace(static_cast<unsigned char>(c));
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](char c) {
                      return std::isspace(static_cast<unsigned char>(c));
                    }).base();
  if (first >= last)
    return {};
  return std::string(first, last);
}

fs::path configDirectory() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
    const fs::path path(xdg);
    if (path.is_absolute())
      return path;
  }
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / ".config";
  return {};
}

std::vector<fs::path> dataDirectories() {
  std::vector<fs::path> directories;
  if (const char *xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
    const fs::path path(xdg);
    if (path.is_absolute())
      directories.push_back(path);
  } else if (const char *home = std::getenv("HOME"); home && *home) {
    directories.push_back(fs::path(home) / ".local/share");
  }

  const char *systemDirectories = std::getenv("XDG_DATA_DIRS");
  std::string_view list = systemDirectories && *systemDirectories
                              ? systemDirectories
                              : "/usr/local/share:/usr/share";
  std::size_t start = 0;
  while (start <= list.size()) {
    const auto separator = list.find(':', start);
    const auto entry = list.substr(start, separator == std::string_view::npos
                                              ? list.size() - start
                                              : separator - start);
    if (!entry.empty()) {
      const fs::path path(entry);
      if (path.is_absolute())
        directories.push_back(path);
    }
    if (separator == std::string_view::npos)
      break;
    start = separator + 1;
  }
  return directories;
}

fs::path locateData(const fs::path &relative) {
  std::error_code error;
  for (const auto &root : dataDirectories()) {
    const auto candidate = root / relative;
    if (fs::is_directory(candidate, error) && !error)
      return candidate;
    error.clear();
  }
  return {};
}

std::string iniValue(const fs::path &file, std::string_view wantedSection,
                     std::string_view wantedKey) {
  std::ifstream input(file);
  std::string section;
  std::string line;
  while (std::getline(input, line)) {
    const auto cleaned = trim(line);
    if (cleaned.empty() || cleaned.front() == '#' || cleaned.front() == ';')
      continue;
    if (cleaned.front() == '[' && cleaned.back() == ']') {
      section = trim(cleaned.substr(1, cleaned.size() - 2));
      continue;
    }
    if (section != wantedSection)
      continue;
    const auto separator = cleaned.find('=');
    if (separator == std::string::npos ||
        trim(cleaned.substr(0, separator)) != wantedKey)
      continue;
    return trim(cleaned.substr(separator + 1));
  }
  return {};
}

std::string detectedIconTheme() {
  if (const char *existing = std::getenv("QS_ICON_THEME");
      existing && *existing)
    return {};

  const auto config = configDirectory();
  std::vector<std::string> candidates;
  const auto kde = iniValue(config / "kdeglobals", "Icons", "Theme");
  const auto gtk = iniValue(config / "gtk-3.0/settings.ini", "Settings",
                            "gtk-icon-theme-name");
  if (!kde.empty())
    candidates.push_back(kde);
  if (!gtk.empty())
    candidates.push_back(gtk);
  for (const auto *fallback : {"breeze", "Adwaita", "Papirus", "hicolor"})
    candidates.emplace_back(fallback);

  const auto roots = dataDirectories();
  for (const auto &name : candidates) {
    for (const auto &root : roots) {
      std::error_code error;
      if (fs::is_regular_file(root / "icons" / name / "index.theme", error) &&
          !error)
        return name;
    }
  }
  return {};
}

void applyProxyEnvironment(ClientEnvironment &environment,
                           const nlohmann::json &preferences) {
  for (const auto *name : kProxyVariables)
    environment.erase(name);
  if (!preferences.value("proxyEnabled", false))
    return;

  const auto insertPair = [&environment, &preferences](const char *key,
                                                       const char *lower,
                                                       const char *upper) {
    auto value = preferences.value(key, std::string{});
    const auto first = std::find_if_not(value.begin(), value.end(), [](char c) {
      return std::isspace(static_cast<unsigned char>(c));
    });
    const auto last =
        std::find_if_not(value.rbegin(), value.rend(), [](char c) {
          return std::isspace(static_cast<unsigned char>(c));
        }).base();
    if (first >= last)
      return;
    value = std::string(first, last);
    environment[lower] = value;
    environment[upper] = std::move(value);
  };
  insertPair("proxyHttp", "http_proxy", "HTTP_PROXY");
  insertPair("proxyHttps", "https_proxy", "HTTPS_PROXY");
  insertPair("proxySocks", "all_proxy", "ALL_PROXY");
  insertPair("proxyBypass", "no_proxy", "NO_PROXY");
}

} // namespace

ClientEnvironment createClientEnvironment(const std::string &socketName,
                                          const std::string &controlPath,
                                          const std::string &binaryDirectory,
                                          const nlohmann::json &preferences) {
  ClientEnvironment environment;
  for (char **entry = environ; entry && *entry; ++entry) {
    const std::string_view item(*entry);
    const auto separator = item.find('=');
    if (separator != std::string_view::npos && separator > 0)
      environment.emplace(std::string(item.substr(0, separator)),
                          std::string(item.substr(separator + 1)));
  }

  for (const auto *name : {"DISPLAY", "XAUTHORITY", "QT_QPA_EGLFS_INTEGRATION",
                           "KITTY_DISABLE_WAYLAND"})
    environment.erase(name);
  environment["WAYLAND_DISPLAY"] = socketName;
  environment["QT_QPA_PLATFORM"] = "wayland;xcb";
  environment["GDK_BACKEND"] = "wayland,x11";
  environment["SDL_VIDEODRIVER"] = "wayland,x11";
  environment["GLFW_PLATFORM"] = "wayland";
  environment["XDG_SESSION_TYPE"] = "wayland";
  environment["XDG_CURRENT_DESKTOP"] = "LunaDash";
  environment["XDG_SESSION_DESKTOP"] = "LunaDash";
  environment["GTK_USE_PORTAL"] = "1";
  environment["QT_QPA_PLATFORMTHEME"] = "xdgdesktopportal";
  environment["LUNADASH_CHROMIUM_WAYLAND"] = "1";
  environment["ELECTRON_OZONE_PLATFORM_HINT"] = "wayland";
  environment["XMODIFIERS"] = "@im=fcitx";
  environment["QT_IM_MODULE"] = "fcitx";
  // wlroots handles text-input-v3/input-method-v2. Toolkit input modules stay
  // on the client side, so Qt and GTK clients can use their native Fcitx paths.
  environment["QT_IM_MODULES"] = "fcitx;wayland";
  environment["GTK_IM_MODULE"] = "fcitx";
  environment["SDL_IM_MODULE"] = "fcitx";
  environment["INPUT_METHOD"] = "fcitx";
  applyProxyEnvironment(environment, preferences);

  auto assetDirectory = locateData("lunadash/data/assets");
  if (assetDirectory.empty())
    assetDirectory = LUDASH_ASSET_SOURCE_DIR;
  environment["LUNADASH_ASSET_DIR"] = assetDirectory.string();

  auto wallpaperDirectory = locateData("ludash/wallpapers");
  if (wallpaperDirectory.empty())
    wallpaperDirectory = assetDirectory.parent_path() / "wallpapers";
  environment["LUNADASH_WALLPAPER_DIR"] = wallpaperDirectory.string();

  environment["LUNADASH_BIN_DIR"] = binaryDirectory;
  environment["LUNADASH_CONTROL"] = controlPath;
  environment["LUDASH_BIN_DIR"] = binaryDirectory;
  environment["LUDASH_CONTROL"] = controlPath;
  const auto iconTheme = detectedIconTheme();
  if (!iconTheme.empty())
    environment["QS_ICON_THEME"] = iconTheme;

  // Client toolkits select their renderer. Quickshell logging stays scoped to
  // the client environment rather than compositor-wide rendering policy.
  auto &loggingRules = environment["QT_LOGGING_RULES"];
  if (!loggingRules.empty())
    loggingRules += ';';
  loggingRules += "quickshell.desktopentry.warning=false";
  return environment;
}

bool publishClientEnvironment(const ClientEnvironment &environment) {
  bool published = true;
  for (const auto &[name, value] : environment)
    if (::setenv(name.c_str(), value.c_str(), 1) != 0)
      published = false;
  return published;
}

} // namespace LunaDash
