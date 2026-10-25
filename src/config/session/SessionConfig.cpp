#include "config/session/SessionConfig.hpp"

#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>

namespace LunaDash {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::size_t kMaxAutostart = 32;
constexpr std::size_t kMaxArguments = 64;

std::optional<std::vector<std::string>> command(const Json &value) {
  if (!value.is_array() || value.empty() || value.size() > kMaxArguments)
    return std::nullopt;
  std::vector<std::string> result;
  for (const auto &argument : value) {
    if (!argument.is_string())
      return std::nullopt;
    result.push_back(argument.get<std::string>());
  }
  if (result.front().empty())
    return std::nullopt;
  return result;
}

std::vector<fs::path> candidates() {
  std::vector<fs::path> paths;
  const char *home = std::getenv("XDG_CONFIG_HOME");
  if (home && *home && fs::path(home).is_absolute())
    paths.push_back(fs::path(home) / "LunaDash/session.json");
  else if (const char *user = std::getenv("HOME"); user && *user)
    paths.push_back(fs::path(user) / ".config/LunaDash/session.json");
  const char *dirs = std::getenv("XDG_CONFIG_DIRS");
  std::stringstream list(dirs && *dirs ? dirs : "/etc/xdg");
  for (std::string directory; std::getline(list, directory, ':');)
    if (!directory.empty() && fs::path(directory).is_absolute())
      paths.push_back(fs::path(directory) / "LunaDash/session.json");
  return paths;
}
} // namespace

const char *sessionShellName(SessionConfig::Shell shell) {
  switch (shell) {
  case SessionConfig::Shell::Custom:
    return "custom";
  case SessionConfig::Shell::None:
    return "none";
  case SessionConfig::Shell::LunaDash:
    break;
  }
  return "lunadash";
}

Json SessionConfig::describe() const {
  return {{"shell", sessionShellName(shell)},
          {"shellCommand", shellCommand},
          {"launcherCommand", launcherCommand},
          {"autostart", autostart},
          {"source", source.string()},
          {"error", error}};
}

SessionConfig parseSessionConfig(const Json &document) {
  SessionConfig config;
  const auto reject = [](std::string message) {
    SessionConfig defaults;
    defaults.error = std::move(message);
    return defaults;
  };
  if (!document.is_object())
    return reject("session.json must contain a JSON object.");
  for (const auto &[key, value] : document.items()) {
    (void)value;
    if (key != "shell" && key != "shellCommand" && key != "launcherCommand" &&
        key != "autostart")
      return reject("Unknown session.json key: " + key);
  }
  if (const auto shell = document.find("shell"); shell != document.end()) {
    if (*shell == "lunadash")
      config.shell = SessionConfig::Shell::LunaDash;
    else if (*shell == "custom")
      config.shell = SessionConfig::Shell::Custom;
    else if (*shell == "none")
      config.shell = SessionConfig::Shell::None;
    else
      return reject("\"shell\" must be \"lunadash\", \"custom\" or \"none\".");
  }
  if (const auto value = document.find("shellCommand"); value != document.end()) {
    const auto parsed = command(*value);
    if (!parsed)
      return reject("\"shellCommand\" must be a non-empty array of strings.");
    config.shellCommand = *parsed;
  }
  if (config.shell == SessionConfig::Shell::Custom && config.shellCommand.empty())
    return reject("A custom shell needs \"shellCommand\".");
  if (const auto value = document.find("launcherCommand"); value != document.end()) {
    const auto parsed = command(*value);
    if (!parsed)
      return reject("\"launcherCommand\" must be a non-empty array of strings.");
    config.launcherCommand = *parsed;
  }
  if (const auto value = document.find("autostart"); value != document.end()) {
    if (!value->is_array() || value->size() > kMaxAutostart)
      return reject("\"autostart\" must be an array of at most 32 commands.");
    for (const auto &entry : *value) {
      const auto parsed = command(entry);
      if (!parsed)
        return reject("Every \"autostart\" entry must be a non-empty array of strings.");
      config.autostart.push_back(*parsed);
    }
  }
  return config;
}

SessionConfig loadSessionConfig() {
  SessionConfig config;
  for (const auto &path : candidates()) {
    std::error_code error;
    if (!fs::is_regular_file(path, error))
      continue;
    std::ifstream file(path);
    const auto document = Json::parse(file, nullptr, false);
    config = document.is_discarded()
                 ? SessionConfig{.error = "session.json is not valid JSON."}
                 : parseSessionConfig(document);
    config.source = path;
    break;
  }
  if (const char *shell = std::getenv("LUNADASH_SHELL"); shell && *shell) {
    if (std::string_view(shell) == "none")
      config.shell = SessionConfig::Shell::None;
    else if (std::string_view(shell) == "lunadash")
      config.shell = SessionConfig::Shell::LunaDash;
  }
  return config;
}

} // namespace LunaDash
