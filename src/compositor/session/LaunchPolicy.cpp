#include "compositor/session/LaunchPolicy.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

namespace LunaDash {
namespace {
using Json = nlohmann::json;

bool equalIgnoringCase(std::string_view left, std::string_view right) {
  if (left.size() != right.size())
    return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    const auto a = static_cast<unsigned char>(left[index]);
    const auto b = static_cast<unsigned char>(right[index]);
    if (std::tolower(a) != std::tolower(b))
      return false;
  }
  return true;
}

std::string normalizedDesktopId(std::string value) {
  value = std::filesystem::path(value).filename().string();
  constexpr std::string_view suffix = ".desktop";
  if (value.size() >= suffix.size() &&
      equalIgnoringCase(std::string_view(value).substr(value.size() - suffix.size()),
                        suffix))
    value.resize(value.size() - suffix.size());
  return value;
}

bool validFlatpakId(std::string_view value) {
  if (value.empty() || !std::isalpha(static_cast<unsigned char>(value.front())))
    return false;
  std::size_t components = 1;
  bool componentHasCharacter = false;
  for (const unsigned char character : value) {
    if (character == '.') {
      if (!componentHasCharacter)
        return false;
      ++components;
      componentHasCharacter = false;
      continue;
    }
    if (!(std::isalnum(character) || character == '_' || character == '-'))
      return false;
    componentHasCharacter = true;
  }
  return componentHasCharacter && components >= 3;
}

std::string flatpakId(const std::vector<std::string> &command) {
  if (command.empty() ||
      !equalIgnoringCase(std::filesystem::path(command.front()).filename().string(),
                         "flatpak"))
    return {};

  std::size_t run = 0;
  while (run < command.size() && command[run] != "run")
    ++run;
  if (run == command.size())
    return {};

  // Flatpak IDs are reverse-DNS-style identifiers. Requiring at least two
  // dots avoids treating option values such as an architecture or branch as
  // the application identity.
  for (std::size_t index = run + 1; index < command.size(); ++index) {
    const auto &value = command[index];
    if (value == "--" || value.starts_with('-'))
      continue;
    if (validFlatpakId(value))
      return value;
  }
  return {};
}

Json applicationsFrom(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return Json::array();
  std::string bytes(65537, '\0');
  file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  const auto size = static_cast<std::size_t>(file.gcount());
  if (size > 65536)
    return Json::array();
  bytes.resize(size);
  const auto document = Json::parse(bytes, nullptr, false);
  if (document.is_discarded() || !document.is_object() ||
      !document.contains("schemaVersion") ||
      !document["schemaVersion"].is_number_integer() ||
      document["schemaVersion"] != 1 ||
      !document.contains("applications") ||
      !document["applications"].is_array())
    return Json::array();
  return document["applications"];
}

bool listContains(const Json &value, const std::string &needle,
                  bool desktopId = false) {
  if (needle.empty() || !value.is_array())
    return false;
  for (const auto &entry : value) {
    if (!entry.is_string())
      continue;
    const std::string candidate = desktopId
                                     ? normalizedDesktopId(entry.get<std::string>())
                                     : entry.get<std::string>();
    if (equalIgnoringCase(candidate, needle))
      return true;
  }
  return false;
}

void applyRules(const Json &rules, const LaunchIdentity &identity,
                LaunchCapabilities *capabilities) {
  for (const auto &rule : rules) {
    if (!rule.is_object())
      continue;
    const bool matches =
        listContains(rule.value("desktopIds", Json::array()), identity.desktopId,
                     true) ||
        listContains(rule.value("flatpakIds", Json::array()), identity.flatpakId) ||
        listContains(rule.value("executables", Json::array()),
                     identity.executable);
    if (!matches)
      continue;
    const auto declared = rule.value("capabilities", Json::array());
    if (!declared.is_array())
      continue;
    for (const auto &capability : declared)
      if (capability.is_string() && capability == "x11-helper")
        capabilities->x11Helper = true;
  }
}

std::filesystem::path configRoot() {
  if (const char *configured = std::getenv("XDG_CONFIG_HOME");
      configured && *configured)
    return configured;
  if (const char *home = std::getenv("HOME"); home && *home)
    return std::filesystem::path(home) / ".config";
  return {};
}

} // namespace

LaunchIdentity identifyLaunch(const std::string &desktopId,
                              const std::vector<std::string> &command) {
  LaunchIdentity identity;
  identity.desktopId = normalizedDesktopId(desktopId);
  if (!command.empty())
    identity.executable = std::filesystem::path(command.front()).filename().string();
  identity.flatpakId = flatpakId(command);
  return identity;
}

LaunchCapabilities resolveLaunchCapabilities(
    const std::string &desktopId, const std::vector<std::string> &command) {
  const auto identity = identifyLaunch(desktopId, command);
  LaunchCapabilities capabilities;

  const std::array bundledPaths{
      std::filesystem::path(LUDASH_LAUNCH_CAPABILITIES_INSTALL_FILE),
      std::filesystem::path(LUDASH_LAUNCH_CAPABILITIES_SOURCE_FILE)};
  for (const auto &path : bundledPaths) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error))
      continue;
    applyRules(applicationsFrom(path), identity, &capabilities);
    break;
  }

  const auto root = configRoot();
  if (!root.empty()) {
    applyRules(applicationsFrom(root / "lunadash/launch-capabilities.json"),
               identity, &capabilities);
  }
  return capabilities;
}

} // namespace LunaDash
