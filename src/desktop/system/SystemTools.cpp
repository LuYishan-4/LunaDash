#include "desktop/system/SystemTools.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonObject>
#include <QStandardPaths>

namespace LunaDash {
namespace {
struct Tool {
  QString id;
  QString category;
  QString name;
  QString package;
  QList<QStringList> commands;
  bool host = false;
};

QList<Tool> tools() {
  return {
      {"host-display",
       "display",
       "Host monitor settings",
       "kscreen or gnome-control-center",
       {{"kcmshell6", "kcm_kscreen"}, {"gnome-control-center", "display"}},
       true},
      {"host-mouse",
       "input",
       "Host mouse settings",
       "plasma-desktop or gnome-control-center",
       {{"kcmshell6", "kcm_mouse"}, {"gnome-control-center", "mouse"}},
       true},
      {"host-touchpad",
       "input",
       "Host touchpad settings",
       "plasma-desktop",
       {{"kcmshell6", "kcm_touchpad"}},
       true},
      {"host-power",
       "power",
       "Host power management",
       "powerdevil or gnome-control-center",
       {{"kcmshell6", "kcm_powerdevilprofilesconfig"},
        {"gnome-control-center", "power"}},
       true},
      {"host-access",
       "privacy",
       "Host accessibility settings",
       "plasma-desktop or gnome-control-center",
       {{"kcmshell6", "kcm_access"},
        {"gnome-control-center", "universal-access"}},
       true},
      {"host-lock",
       "privacy",
       "Host screen locking settings",
       "kscreenlocker",
       {{"kcmshell6", "kcm_screenlocker"}},
       true},
      {"network",
       "network",
       "Network connection editor",
       "nm-connection-editor",
       {{"nm-connection-editor"}}},
      {"firewall",
       "network",
       "Firewall rules and zones",
       "firewall-config or gufw",
       {{"firewall-config"}, {"gufw"}}},
      {"certificates",
       "network",
       "Certificates and keys",
       "kleopatra or seahorse",
       {{"kleopatra"}, {"seahorse"}}},
      {"audio",
       "sound",
       "Audio devices and routing",
       "pavucontrol",
       {{"pavucontrol"}, {"pwvucontrol"}}},
      {"bluetooth",
       "bluetooth",
       "Bluetooth devices",
       "blueman",
       {{"blueman-manager"}}},
      {"ime",
       "input",
       "Input methods",
       "fcitx5 + fcitx5-configtool",
       {{"fcitx5-configtool"}}},
      {"printers",
       "devices",
       "Printers and scanners",
       "system-config-printer",
       {{"system-config-printer"}}},
      {"hardware",
       "devices",
       "Detailed hardware information",
       "hardinfo2 or lshw-gui",
       {{"hardinfo2"}, {"hardinfo"}, {"lshw-gtk"}}},
      {"storage",
       "devices",
       "Disks, SMART and filesystem operations",
       "gnome-disk-utility",
       {{"gnome-disks"}}},
      {"partitioner",
       "devices",
       "Partition editor",
       "partitionmanager or gparted",
       {{"partitionmanager"}, {"gparted"}}},
      {"packages",
       "devices",
       "Driver and system package updates",
       "discover, pamac-manager or bauh",
       {{"plasma-discover", "--mode", "update"},
        {"pamac-manager", "--updates"},
        {"bauh"}}},
      {"users",
       "system",
       "User accounts",
       "plasma-workspace or gnome-control-center",
       {{"kcmshell6", "kcm_users"}, {"gnome-control-center", "user-accounts"}}},
      {"datetime",
       "system",
       "Date and time",
       "plasma-workspace or gnome-control-center",
       {{"kcmshell6", "kcm_clock"}, {"gnome-control-center", "datetime"}}},
      {"defaults",
       "applications",
       "Default applications and file associations",
       "plasma-workspace or xfce4-settings",
       {{"kcmshell6", "kcm_componentchooser"}, {"xfce4-mime-settings"}}}};
}

bool insideLunaDashSession() {
  const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
  const QString display = qEnvironmentVariable("WAYLAND_DISPLAY");
  return !qEnvironmentVariable("LUNADASH_CONTROL").isEmpty() ||
         display.startsWith("lunadash-") || display.startsWith("ludash-") ||
         desktop.contains("LunaDash", Qt::CaseInsensitive) ||
         desktop.contains("LuDash", Qt::CaseInsensitive);
}

QStringList resolve(const Tool &tool) {
  const bool lunaDash = insideLunaDashSession();
  if (tool.host && (lunaDash || qEnvironmentVariable("QT_QPA_PLATFORM") == "eglfs"))
    return {};
  for (auto command : tool.commands) {
    // KDE KCMs assume a Plasma host and can partially initialize under a
    // standalone LunaDash session. Prefer the standalone system tool fallback
    // instead of launching a host-specific control module.
    if (lunaDash && command.first() == "kcmshell6")
      continue;
    if (command.first() == "kcmshell6") {
      bool found = false;
      for (const auto &root : QCoreApplication::libraryPaths())
        for (const auto &folder : {"systemsettings", "systemsettings_qwidgets"})
          if (QFileInfo::exists(root + "/plasma/kcms/" + folder + "/" +
                                command[1] + ".so"))
            found = true;
      if (!found)
        continue;
    }
    const auto executable = QStandardPaths::findExecutable(command.first());
    if (!executable.isEmpty()) {
      command[0] = executable;
      return command;
    }
  }
  return {};
}
} // namespace

QJsonArray systemSettingsTools() {
  QJsonArray result;
  for (const auto &tool : tools()) {
    const auto command = resolve(tool);
    const bool runtimeAvailable =
        tool.id == "ime"
            ? !QStandardPaths::findExecutable(QStringLiteral("fcitx5"))
                   .isEmpty()
            : !command.isEmpty();
    result.append(QJsonObject{{"id", tool.id},
                              {"category", tool.category},
                              {"name", tool.name},
                              {"package", tool.package},
                              {"available", runtimeAvailable},
                              {"configurable", !command.isEmpty()},
                              {"host", tool.host}});
  }
  return result;
}

bool systemSettingsToolUsesHost(const QString &id) {
  for (const auto &tool : tools())
    if (tool.id == id)
      return tool.host;
  return false;
}

QStringList systemSettingsCommand(const QString &id) {
  for (const auto &tool : tools())
    if (tool.id == id)
      return resolve(tool);
  return {};
}
} // namespace LunaDash
