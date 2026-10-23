#include "compositor/session/SessionEnvironment.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {

void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

void writeFile(const std::filesystem::path &path, const std::string &contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path);
  output << contents;
  if (!output)
    throw std::runtime_error("Could not write test configuration file.");
}

class TemporaryTree final {
public:
  TemporaryTree() {
    std::string pattern = "/tmp/lunadash-session-environment-XXXXXX";
    std::vector<char> path(pattern.begin(), pattern.end());
    path.push_back('\0');
    char *created = ::mkdtemp(path.data());
    if (!created)
      throw std::runtime_error("Could not create temporary test directory.");
    path_ = created;
  }

  ~TemporaryTree() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

void prepareEnvironment(const std::filesystem::path &root) {
  const auto home = root / "home";
  const auto config = root / "config";
  const auto data = root / "data";
  std::filesystem::create_directories(home);
  std::filesystem::create_directories(data / "icons/TestTheme");
  std::filesystem::create_directories(data / "lunadash/data/assets");
  std::filesystem::create_directories(data / "ludash/wallpapers");
  writeFile(config / "kdeglobals", "[Icons]\nTheme=TestTheme\n");
  writeFile(data / "icons/TestTheme/index.theme", "[Icon Theme]\nName=Test\n");

  ::setenv("HOME", home.c_str(), 1);
  ::setenv("XDG_CONFIG_HOME", config.c_str(), 1);
  ::setenv("XDG_DATA_HOME", data.c_str(), 1);
  ::setenv("XDG_DATA_DIRS", "/usr/local/share:/usr/share", 1);
  ::setenv("DISPLAY", ":9", 1);
  ::setenv("XAUTHORITY", "/tmp/xauthority-test", 1);
  ::setenv("QT_QPA_EGLFS_INTEGRATION", "test", 1);
  ::setenv("KITTY_DISABLE_WAYLAND", "1", 1);
  ::setenv("QT_LOGGING_RULES", "app.debug=true", 1);
  ::setenv("http_proxy", "inherited proxy", 1);
  ::setenv("HTTPS_PROXY", "inherited secure proxy", 1);
  ::unsetenv("QS_ICON_THEME");
}

void environmentKeepsToolkitCompatibilityAndUsesNativePreferences(
    const TemporaryTree &tree) {
  prepareEnvironment(tree.path());
  const nlohmann::json preferences{
      {"proxyEnabled", true},
      {"proxyHttp", " http://proxy.example:8080 "},
      {"proxyHttps", "https://proxy.example:8443"},
      {"proxySocks", "socks5://proxy.example:1080"},
      {"proxyBypass", " localhost,127.0.0.1 "}};
  const auto environment = createClientEnvironment(
      "wayland-lunadash-test", "/run/user/1000/lunadash-control",
      "/opt/lunadash/bin", preferences);

  require(!environment.contains("DISPLAY"),
          "X11 display leaked into the Wayland client environment.");
  require(!environment.contains("XAUTHORITY"),
          "X11 authorization leaked into the Wayland client environment.");
  require(!environment.contains("QT_QPA_EGLFS_INTEGRATION"),
          "EGLFS override leaked into the client environment.");
  require(!environment.contains("KITTY_DISABLE_WAYLAND"),
          "Kitty Wayland override leaked into the client environment.");
  require(environment.at("WAYLAND_DISPLAY") == "wayland-lunadash-test",
          "Wayland socket was not published to clients.");
  require(environment.at("QT_QPA_PLATFORM") == "wayland;xcb" &&
              environment.at("GDK_BACKEND") == "wayland,x11" &&
              environment.at("SDL_VIDEODRIVER") == "wayland,x11" &&
              environment.at("GLFW_PLATFORM") == "wayland",
          "Toolkit-specific Wayland compatibility settings changed.");
  require(environment.at("QT_IM_MODULES") == "fcitx;wayland" &&
              environment.at("GTK_IM_MODULE") == "fcitx" &&
              environment.at("SDL_IM_MODULE") == "fcitx",
          "Client-side input method settings changed.");
  require(environment.at("http_proxy") == "http://proxy.example:8080" &&
              environment.at("HTTP_PROXY") == "http://proxy.example:8080" &&
              environment.at("https_proxy") == "https://proxy.example:8443" &&
              environment.at("all_proxy") == "socks5://proxy.example:1080" &&
              environment.at("no_proxy") == "localhost,127.0.0.1",
          "Saved proxy settings were not applied consistently.");
  require(environment.at("LUNADASH_ASSET_DIR") ==
                  (tree.path() / "data/lunadash/data/assets").string() &&
              environment.at("LUNADASH_WALLPAPER_DIR") ==
                  (tree.path() / "data/ludash/wallpapers").string(),
          "XDG data paths were not discovered.");
  require(environment.at("QS_ICON_THEME") == "TestTheme",
          "Configured icon theme was not detected.");
  require(environment.at("QT_LOGGING_RULES") ==
              "app.debug=true;quickshell.desktopentry.warning=false",
          "Quickshell logging rule did not preserve existing client rules.");
  require(environment.at("LUNADASH_CONTROL") ==
                  "/run/user/1000/lunadash-control" &&
              environment.at("LUDASH_BIN_DIR") == "/opt/lunadash/bin",
          "LunaDash client paths were not exported.");

  ::setenv("http_proxy", "inherited proxy", 1);
  const auto disabled = createClientEnvironment(
      "wayland-test", "/control", "/bin", {{"proxyEnabled", false}});
  for (const auto *name :
       {"http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY", "all_proxy",
        "ALL_PROXY", "no_proxy", "NO_PROXY"})
    require(!disabled.contains(name),
            "Inherited proxy bypassed the disabled proxy preference.");
}

void clientEnvironmentCanBePublished() {
  constexpr const char *name = "LUNADASH_SESSION_ENVIRONMENT_TEST";
  ::unsetenv(name);
  require(publishClientEnvironment({{name, "published"}}),
          "Client environment publication failed.");
  const char *value = std::getenv(name);
  require(value && std::string(value) == "published",
          "Client environment was not published to the compositor process.");
  ::unsetenv(name);
}

} // namespace
} // namespace LunaDash

int main() {
  try {
    LunaDash::TemporaryTree tree;
    LunaDash::environmentKeepsToolkitCompatibilityAndUsesNativePreferences(
        tree);
    LunaDash::clientEnvironmentCanBePublished();
  } catch (const std::exception &error) {
    std::cerr << "Session environment test failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
