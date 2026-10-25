#include "config/session/SessionConfig.hpp"

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
using Json = nlohmann::json;

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

void parsing() {
  const auto defaults = parseSessionConfig(Json::object());
  require(defaults.shell == SessionConfig::Shell::LunaDash && defaults.error.empty(),
          "An empty session.json keeps the LunaDash shell.");

  const auto custom = parseSessionConfig(Json::parse(R"({
    "shell": "custom", "shellCommand": ["waybar", "-c", "x"],
    "launcherCommand": ["fuzzel"], "autostart": [["mako"], ["swaybg", "-c", "#202020"]]})"));
  require(custom.error.empty() && custom.shell == SessionConfig::Shell::Custom,
          "A custom shell with a command is accepted.");
  require(custom.shellCommand.size() == 3 && custom.launcherCommand.front() == "fuzzel" &&
              custom.autostart.size() == 2 && custom.autostart[1][2] == "#202020",
          "Commands keep their arguments.");

  const auto none = parseSessionConfig(Json::parse(R"({"shell": "none"})"));
  require(none.error.empty() && none.shell == SessionConfig::Shell::None,
          "\"none\" runs no shell.");

  for (const char *invalid : {
           R"({"shell": "custom"})",
           R"({"shell": "plasma"})",
           R"({"shell": "custom", "shellCommand": "waybar"})",
           R"({"shell": "none", "autostart": [["mako"], []]})",
           R"({"shell": "none", "autostart": [[""]]})",
           R"({"shell": "none", "exec": ["mako"]})",
           R"(["lunadash"])",
       }) {
    const auto rejected = parseSessionConfig(Json::parse(invalid));
    require(!rejected.error.empty() && rejected.shell == SessionConfig::Shell::LunaDash &&
                rejected.autostart.empty(),
            invalid);
  }
}

void loading() {
  char pattern[] = "/tmp/lunadash-session-config-XXXXXX";
  const std::filesystem::path root = ::mkdtemp(pattern);
  ::setenv("XDG_CONFIG_HOME", (root / "user").c_str(), 1);
  ::setenv("XDG_CONFIG_DIRS", (root / "system").c_str(), 1);
  ::unsetenv("LUNADASH_SHELL");

  require(loadSessionConfig().source.empty(), "Without files the defaults apply.");

  writeFile(root / "system/LunaDash/session.json", R"({"shell": "none"})");
  auto config = loadSessionConfig();
  require(config.shell == SessionConfig::Shell::None &&
              config.source == root / "system/LunaDash/session.json",
          "The system default applies without a user file.");

  writeFile(root / "user/LunaDash/session.json",
            R"({"shell": "custom", "shellCommand": ["waybar"]})");
  config = loadSessionConfig();
  require(config.shell == SessionConfig::Shell::Custom &&
              config.source == root / "user/LunaDash/session.json",
          "The user's file wins over the system default.");

  ::setenv("LUNADASH_SHELL", "lunadash", 1);
  require(loadSessionConfig().shell == SessionConfig::Shell::LunaDash,
          "LUNADASH_SHELL overrides the file for one session.");
  ::unsetenv("LUNADASH_SHELL");

  writeFile(root / "user/LunaDash/session.json", "{ not json");
  config = loadSessionConfig();
  require(!config.error.empty() && config.shell == SessionConfig::Shell::LunaDash,
          "A broken user file falls back to the LunaDash shell and reports why.");
  std::filesystem::remove_all(root);
}
} // namespace
} // namespace LunaDash

int main() {
  try {
    LunaDash::parsing();
    LunaDash::loading();
  } catch (const std::exception &error) {
    std::cerr << "Session config test failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
