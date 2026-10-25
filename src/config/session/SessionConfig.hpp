#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace LunaDash {

// What a LunaDash session starts besides the compositor, read from session.json:
//
//   {
//     "shell": "lunadash" | "custom" | "none",
//     "shellCommand": ["waybar"],              // required for "custom"
//     "launcherCommand": ["fuzzel"],           // Super / launcher shortcut without the
//                                              // LunaDash shell
//     "autostart": [["mako"], ["swaybg", "-c", "#202020"]]
//   }
//
// The user's $XDG_CONFIG_HOME/LunaDash/session.json wins over the first
// $XDG_CONFIG_DIRS/LunaDash/session.json (the installer's system default). LUNADASH_SHELL
// (lunadash, none) overrides the shell for one session.
struct SessionConfig {
  enum class Shell { LunaDash, Custom, None };

  Shell shell = Shell::LunaDash;
  std::vector<std::string> shellCommand;
  std::vector<std::string> launcherCommand;
  std::vector<std::vector<std::string>> autostart;
  // The file that was read, empty when none exists.
  std::filesystem::path source;
  // Why the file was rejected; the defaults are used then.
  std::string error;

  nlohmann::json describe() const;
};

const char *sessionShellName(SessionConfig::Shell shell);

// Validates a parsed session.json; on failure `error` says why and the defaults are kept.
SessionConfig parseSessionConfig(const nlohmann::json &document);

SessionConfig loadSessionConfig();

} // namespace LunaDash
