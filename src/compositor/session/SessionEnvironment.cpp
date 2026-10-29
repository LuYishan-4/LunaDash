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
