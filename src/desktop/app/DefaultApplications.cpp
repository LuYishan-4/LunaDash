#include "desktop/app/DefaultApplications.hpp"

#include "desktop/browser/Browser.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <unistd.h>

namespace LunaDash {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using Command = std::vector<std::string>;

constexpr std::array<std::string_view, 3> roles{"terminal", "files", "browser"};

void setError(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
}

fs::path configRoot() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME");
      xdg && *xdg && fs::path(xdg).is_absolute())
    return xdg;
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / ".config";
  return {};
}

fs::path dataRoot() {
  if (const char *xdg = std::getenv("XDG_DATA_HOME");
      xdg && *xdg && fs::path(xdg).is_absolute())
    return xdg;
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / ".local/share";
  return {};
}

bool executableAvailable(std::string_view executable) {
  if (executable.empty())
    return false;
  const fs::path program(executable);
  if (program.has_parent_path())
    return ::access(program.c_str(), X_OK) == 0;

  const char *pathValue = std::getenv("PATH");
  if (!pathValue)
    return false;
  const std::string path(pathValue);
  std::size_t begin = 0;
  while (begin <= path.size()) {
    const auto end = path.find(':', begin);
    const auto component = path.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin);
    const fs::path directory =
        component.empty() ? fs::current_path() : fs::path(component);
    if (::access((directory / program).c_str(), X_OK) == 0)
      return true;
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return false;
}

std::string basename(std::string_view value) {
  return fs::path(value).filename().string();
}

bool validCommand(const Json &value) {
  if (!value.is_array() || value.size() > 24)
    return false;
  for (const auto &argument : value) {
    if (!argument.is_string())
      return false;
    const auto text = argument.get<std::string>();
    if (text.size() > 1024 || text.find('\0') != std::string::npos)
      return false;
  }
  if (value.empty())
    return true;
  const auto program = value.front().get<std::string>();
  if (program.empty() || program.starts_with('-'))
    return false;
  const auto name = basename(program);
  return name != "ludash-desktop" && name != "ludashctl" &&
         name != "lunadash-desktop" && name != "lunadashctl";
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

// QSettings writes QStringList values as comma-separated fields with quoted
// entries when needed. Read that legacy spelling so existing defaults remain
// available after the native JSON store is introduced.
Command parseLegacyStringList(std::string_view source) {
  const auto text = trim(std::string(source));
  if (text.empty() || text == "@Invalid()")
    return {};
  Command result;
  std::string item;
  bool quoted = false;
  bool escaped = false;
  bool fieldWasQuoted = false;
  for (const char character : text) {
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
      if (!quoted && trim(item).empty())
        item.clear();
      quoted = !quoted;
      fieldWasQuoted = true;
    } else if (character == ',' && !quoted) {
      result.push_back(fieldWasQuoted ? item : trim(item));
      item.clear();
      fieldWasQuoted = false;
    } else {
      item.push_back(character);
    }
  }
  if (escaped)
    item.push_back('\\');
  result.push_back(fieldWasQuoted ? item : trim(item));
  return result;
}

Command readLegacyRole(std::string_view role) {
  const auto root = configRoot();
  if (root.empty())
    return {};
  for (const auto &filename :
       {"LunaDash/LunaDash.conf", "LunaDash/LunaDash.ini"}) {
    std::ifstream input(root / filename);
    std::string line;
    bool inDefaults = false;
    while (std::getline(input, line)) {
      const auto entry = trim(line);
      if (entry.empty() || entry.front() == '#' || entry.front() == ';')
        continue;
      if (entry.front() == '[' && entry.back() == ']') {
        inDefaults = entry == "[defaultApps]";
        continue;
      }
      if (!inDefaults)
        continue;
      const auto separator = entry.find('=');
      if (separator != std::string::npos &&
          trim(entry.substr(0, separator)) == role)
        return parseLegacyStringList(entry.substr(separator + 1));
    }
  }
  return {};
}

Json readConfigured() {
  Json result = Json::object();
  const auto root = configRoot();
  if (root.empty())
    return result;

  std::ifstream input(root / "LunaDash/default-applications.json");
  if (input) {
    try {
      const auto stored = Json::parse(input);
      if (stored.is_object())
        for (const auto role : roles)
          if (stored.contains(role) && validCommand(stored[role]))
            result[role] = stored[role];
    } catch (const Json::exception &) {
      // A damaged native settings file should not hide recoverable legacy
      // settings. Invalid values are ignored and defaults remain available.
    }
  }

  for (const auto role : roles)
    if (!result.contains(role)) {
      const auto legacy = readLegacyRole(role);
      if (validCommand(legacy))
        result[role] = legacy;
    }
  return result;
}

bool writeAll(int descriptor, std::string_view data) {
  while (!data.empty()) {
    const auto written = ::write(descriptor, data.data(), data.size());
    if (written < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    data.remove_prefix(static_cast<std::size_t>(written));
  }
  return true;
}

bool writeConfigured(const Json &settings, std::string *error) {
  const auto root = configRoot();
  if (root.empty()) {
    setError(error, "Could not locate the XDG configuration directory.");
    return false;
  }
  const auto directory = root / "LunaDash";
  std::error_code filesystemError;
  fs::create_directories(directory, filesystemError);
  if (filesystemError) {
    setError(error,
             "Could not create the application configuration directory.");
    return false;
  }

  const auto destination = directory / "default-applications.json";
  const auto temporary = destination.string() + ".tmp." +
                         std::to_string(static_cast<long long>(::getpid()));
  const int descriptor =
      ::open(temporary.c_str(),
             O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (descriptor < 0) {
    setError(error, "Could not open the default-application settings file.");
    return false;
  }

  const auto serialized = settings.dump(2) + "\n";
  const bool written =
      writeAll(descriptor, serialized) && ::fsync(descriptor) == 0;
  const int closeResult = ::close(descriptor);
  if (!written || closeResult != 0 ||
      ::rename(temporary.c_str(), destination.c_str()) != 0) {
    const int savedError = errno;
    ::unlink(temporary.c_str());
    setError(error, "Could not save default-application settings: " +
                        std::string(std::strerror(savedError)));
    return false;
  }
  return true;
}

std::string readIniValue(const fs::path &file, std::string_view section,
                         std::string_view key) {
  std::ifstream input(file);
  std::string line;
  bool inSection = false;
  while (std::getline(input, line)) {
    const auto entry = trim(line);
    if (entry.empty() || entry.front() == '#' || entry.front() == ';')
      continue;
    if (entry.front() == '[' && entry.back() == ']') {
      inSection = entry.substr(1, entry.size() - 2) == section;
      continue;
    }
    if (!inSection)
      continue;
    const auto separator = entry.find('=');
    if (separator != std::string::npos &&
        trim(entry.substr(0, separator)) == key)
      return trim(entry.substr(separator + 1));
  }
  return {};
}

Command konsoleCommandForProfile(const fs::path &configDirectory,
                                 const Command &launcher) {
  const auto profile = readIniValue(configDirectory / "konsolerc",
                                    "Desktop Entry", "DefaultProfile");
  if (profile.empty())
    return {};
  const auto profileFile = configDirectory / "../data/konsole" / profile;
  if (!fs::exists(profileFile))
    return {};
  Command command = launcher;
  command.insert(command.end(), {"--profile", fs::path(profile).stem().string(),
                                 "--separate"});
  return command;
}

Command userKonsoleCommand() {
  const auto root = configRoot();
  const auto home =
      std::getenv("HOME") ? fs::path(std::getenv("HOME")) : fs::path{};
  if (!home.empty() && executableAvailable("flatpak")) {
    auto flatpak =
        konsoleCommandForProfile(home / ".var/app/org.kde.konsole/config",
                                 {"flatpak", "run", "org.kde.konsole"});
    if (!flatpak.empty())
      return flatpak;
  }

  if (root.empty())
    return {"konsole", "--separate"};
  const auto profile =
      readIniValue(root / "konsolerc", "Desktop Entry", "DefaultProfile");
  if (profile.empty())
    return {"konsole", "--separate"};
  const auto data = dataRoot();
  const auto profileFile =
      data.empty() ? fs::path{} : data / "konsole" / profile;
  if (fs::exists(profileFile))
    return {"konsole", "--profile", fs::path(profile).stem().string(),
            "--separate"};
  return {"konsole", "--profile", profile, "--separate"};
}

Command normalizeKonsoleCommand(const Command &command) {
  if (command.empty() || basename(command.front()) != "konsole" ||
      std::find(command.begin(), command.end(), "--profile") != command.end() ||
      std::find(command.begin(), command.end(), "--separate") !=
          command.end() ||
      std::find(command.begin(), command.end(), "-e") != command.end())
    return command;
  auto result = userKonsoleCommand();
  result.insert(result.end(), std::next(command.begin()), command.end());
  return result;
}

} // namespace

Json defaultApplications() {
  Json result = Json::object();
  const auto configured = readConfigured();
  for (const auto role : roles)
    result[role] = configured.value(role, Json::array());
  return result;
}

bool setDefaultApplications(const Json &changes, std::string *error) {
  if (error)
    error->clear();
  if (!changes.is_object()) {
    setError(error, "Expected a JSON object of application roles.");
    return false;
  }
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if (std::find(roles.begin(), roles.end(), it.key()) == roles.end() ||
        !validCommand(it.value())) {
      setError(error,
               "Defaults require terminal/files/browser argument arrays "
               "(empty selects the LunaDash default), at most 24 strings, "
               "no recursive LunaDash launcher.");
      return false;
    }
    if (!it.value().empty() &&
        !executableAvailable(it.value().front().get<std::string>())) {
      setError(error, "Application executable was not found.");
      return false;
    }
  }

  auto settings = readConfigured();
  for (auto it = changes.begin(); it != changes.end(); ++it)
    settings[it.key()] = it.value();
  return writeConfigured(settings, error);
}

Command defaultApplicationCommand(std::string_view role, std::string *error) {
  if (error)
    error->clear();
  if (std::find(roles.begin(), roles.end(), role) == roles.end()) {
    setError(error, "Unknown application role.");
    return {};
  }

  const auto applications = defaultApplications();
  const auto configured = applications.at(std::string(role));
  if (configured.is_array() && !configured.empty()) {
    Command command = configured.get<Command>();
    return role == "terminal" ? normalizeKonsoleCommand(command) : command;
  }
  if (role == "terminal")
    return {"kitty"};
  if (role == "files")
    return {"dolphin", "--new-window"};
  const auto browser = Browser::defaultCommand();
  if (browser.empty())
    setError(error, "Google Chrome or Chromium is not installed.");
  return browser;
}

Command konsoleCommand() { return userKonsoleCommand(); }

} // namespace LunaDash
