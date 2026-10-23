#include "desktop/system/SystemTools.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {
struct Tool {
  const char *id;
  const char *category;
  const char *name;
  const char *package;
  std::vector<std::vector<std::string>> commands;
  bool host = false;
};

const std::vector<Tool> &tools() {
  static const std::vector<Tool> entries{
      {"host-display", "display", "Host monitor settings",
       "kscreen or gnome-control-center",
       {{"kcmshell6", "kcm_kscreen"}, {"gnome-control-center", "display"}},
       true},
      {"host-mouse", "input", "Host mouse settings",
       "plasma-desktop or gnome-control-center",
       {{"kcmshell6", "kcm_mouse"}, {"gnome-control-center", "mouse"}},
       true},
      {"host-touchpad", "input", "Host touchpad settings", "plasma-desktop",
       {{"kcmshell6", "kcm_touchpad"}}, true},
      {"host-power", "power", "Host power management",
       "powerdevil or gnome-control-center",
       {{"kcmshell6", "kcm_powerdevilprofilesconfig"},
        {"gnome-control-center", "power"}},
       true},
      {"host-access", "privacy", "Host accessibility settings",
       "plasma-desktop or gnome-control-center",
       {{"kcmshell6", "kcm_access"},
        {"gnome-control-center", "universal-access"}},
       true},
      {"host-lock", "privacy", "Host screen locking settings",
       "kscreenlocker", {{"kcmshell6", "kcm_screenlocker"}}, true},
      {"network", "network", "Network connection editor",
       "nm-connection-editor", {{"nm-connection-editor"}}},
      {"firewall", "network", "Firewall rules and zones",
       "firewall-config or gufw", {{"firewall-config"}, {"gufw"}}},
      {"certificates", "network", "Certificates and keys",
       "kleopatra or seahorse", {{"kleopatra"}, {"seahorse"}}},
      {"audio", "sound", "Audio devices and routing", "pavucontrol",
       {{"pavucontrol"}, {"pwvucontrol"}}},
      {"bluetooth", "bluetooth", "Bluetooth devices", "blueman",
       {{"blueman-manager"}}},
      {"ime", "input", "Input methods", "fcitx5 + fcitx5-configtool",
       {{"fcitx5-configtool"}}},
      {"printers", "devices", "Printers and scanners",
       "system-config-printer", {{"system-config-printer"}}},
      {"hardware", "devices", "Detailed hardware information",
       "hardinfo2 or lshw-gui", {{"hardinfo2"}, {"hardinfo"}, {"lshw-gtk"}}},
      {"storage", "devices", "Disks, SMART and filesystem operations",
       "gnome-disk-utility", {{"gnome-disks"}}},
      {"partitioner", "devices", "Partition editor",
       "partitionmanager or gparted", {{"partitionmanager"}, {"gparted"}}},
      {"packages", "devices", "Driver and system package updates",
       "discover, pamac-manager or bauh",
       {{"plasma-discover", "--mode", "update"},
        {"pamac-manager", "--updates"}, {"bauh"}}},
      {"users", "system", "User accounts",
       "plasma-workspace or gnome-control-center",
       {{"kcmshell6", "kcm_users"},
        {"gnome-control-center", "user-accounts"}}},
      {"datetime", "system", "Date and time",
       "plasma-workspace or gnome-control-center",
       {{"kcmshell6", "kcm_clock"}, {"gnome-control-center", "datetime"}}},
      {"defaults", "applications", "Default applications and file associations",
       "plasma-workspace or xfce4-settings",
       {{"kcmshell6", "kcm_componentchooser"}, {"xfce4-mime-settings"}}}};
  return entries;
}

bool executable(const std::filesystem::path &path) {
  return ::access(path.c_str(), X_OK) == 0;
}

std::string findExecutable(std::string_view program) {
  if (program.empty())
    return {};
  const std::filesystem::path requested(program);
  if (requested.has_parent_path())
    return executable(requested) ? requested.string() : std::string{};

  const char *path = std::getenv("PATH");
  if (!path)
    return {};
  const std::string_view searchPath(path);
  std::size_t start = 0;
  while (start <= searchPath.size()) {
    const auto separator = searchPath.find(':', start);
    const auto directory = searchPath.substr(
        start, separator == std::string_view::npos ? searchPath.size() - start
                                                   : separator - start);
    if (!directory.empty()) {
      const auto candidate = std::filesystem::path(directory) / requested;
      if (executable(candidate))
        return candidate.string();
    }
    if (separator == std::string_view::npos)
      break;
    start = separator + 1;
  }
  return {};
}

std::vector<std::filesystem::path> qtPluginPaths() {
  std::vector<std::filesystem::path> paths;
  if (const char *configured = std::getenv("QT_PLUGIN_PATH");
      configured && *configured) {
    const std::string_view list(configured);
    std::size_t start = 0;
    while (start <= list.size()) {
      const auto separator = list.find(':', start);
      const auto entry = list.substr(start,
          separator == std::string_view::npos ? list.size() - start
                                              : separator - start);
      if (!entry.empty())
        paths.emplace_back(entry);
      if (separator == std::string_view::npos)
        break;
      start = separator + 1;
    }
  }
  for (const auto *root : {"/usr/lib/qt6/plugins", "/usr/lib64/qt6/plugins",
                           "/usr/local/lib/qt6/plugins"})
    paths.emplace_back(root);
  return paths;
}

bool hasKdeControlModule(std::string_view module) {
  for (const auto &root : qtPluginPaths()) {
    for (const auto *folder : {"systemsettings", "systemsettings_qwidgets"}) {
      std::error_code error;
      if (std::filesystem::is_regular_file(
              root / "plasma/kcms" / folder / (std::string(module) + ".so"),
              error))
        return true;
    }
  }
  return false;
}

bool containsIgnoringCase(std::string_view text, std::string_view needle) {
  if (needle.size() > text.size())
    return false;
  for (std::size_t start = 0; start <= text.size() - needle.size(); ++start) {
    bool matches = true;
    for (std::size_t offset = 0; offset < needle.size(); ++offset) {
      const auto left = static_cast<unsigned char>(text[start + offset]);
      const auto right = static_cast<unsigned char>(needle[offset]);
      if (std::tolower(left) != std::tolower(right)) {
        matches = false;
        break;
      }
    }
    if (matches)
      return true;
  }
  return false;
}

bool insideLunaDashSession() {
  const char *desktop = std::getenv("XDG_CURRENT_DESKTOP");
  const char *display = std::getenv("WAYLAND_DISPLAY");
  const char *control = std::getenv("LUNADASH_CONTROL");
  const char *compatControl = std::getenv("LUDASH_CONTROL");
  return (control && *control) || (compatControl && *compatControl) ||
         (display && (std::string_view(display).starts_with("lunadash-") ||
                      std::string_view(display).starts_with("ludash-"))) ||
         (desktop && (containsIgnoringCase(desktop, "LunaDash") ||
                      containsIgnoringCase(desktop, "LuDash")));
}

std::vector<std::string> resolve(const Tool &tool) {
  const bool lunaDash = insideLunaDashSession();
  if (tool.host && (lunaDash ||
                    (std::getenv("QT_QPA_PLATFORM") &&
                     std::string_view(std::getenv("QT_QPA_PLATFORM")) ==
                         "eglfs")))
    return {};

  for (auto command : tool.commands) {
    if (command.empty())
      continue;
    if (lunaDash && command.front() == "kcmshell6")
      continue;
    if (command.front() == "kcmshell6" &&
        (command.size() < 2 || !hasKdeControlModule(command[1])))
      continue;
    const auto path = findExecutable(command.front());
    if (!path.empty()) {
      command.front() = path;
      return command;
    }
  }
  return {};
}

} // namespace

std::vector<SystemSettingsTool> systemSettingsTools() {
  std::vector<SystemSettingsTool> result;
  result.reserve(tools().size());
  for (const auto &tool : tools()) {
    const auto command = resolve(tool);
    const bool available =
        std::string_view(tool.id) == "ime" ? !findExecutable("fcitx5").empty()
                                            : !command.empty();
    result.push_back({tool.id, tool.category, tool.name, tool.package, available,
                      !command.empty(), tool.host});
  }
  return result;
}

bool systemSettingsToolUsesHost(std::string_view id) {
  for (const auto &tool : tools())
    if (std::string_view(tool.id) == id)
      return tool.host;
  return false;
}

std::vector<std::string> systemSettingsCommand(std::string_view id) {
  for (const auto &tool : tools())
    if (std::string_view(tool.id) == id)
      return resolve(tool);
  return {};
}

} // namespace LunaDash
