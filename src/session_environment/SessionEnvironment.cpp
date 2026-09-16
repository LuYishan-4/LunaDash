#include <LuDash/session_environment/SessionEnvironment.h>

#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace LuDash {
namespace {
// The shell inherits LunaDash's own desktop identity, so Qt cannot infer the
// host icon theme and every themed icon lookup fails. Resolve a theme
// explicitly and let Quickshell use it. An explicit QS_ICON_THEME always wins.
QString detectedIconTheme() {
  if (!qEnvironmentVariableIsEmpty("QS_ICON_THEME"))
    return {};
  const QString configLocation =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  QStringList candidates;
  const QString kdeTheme =
      QSettings(configLocation + "/kdeglobals", QSettings::IniFormat)
          .value("Icons/Theme")
          .toString();
  if (!kdeTheme.isEmpty())
    candidates.append(kdeTheme);
  const QString gtkTheme =
      QSettings(configLocation + "/gtk-3.0/settings.ini", QSettings::IniFormat)
          .value("Settings/gtk-icon-theme-name")
          .toString();
  if (!gtkTheme.isEmpty())
    candidates.append(gtkTheme);
  candidates.append({"breeze", "Adwaita", "Papirus", "hicolor"});
  candidates.removeDuplicates();
  for (const auto &name : candidates) {
    if (QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                               "icons/" + name + "/index.theme")
            .isEmpty())
      continue;
    return name;
  }
  return {};
}
} // namespace
QProcessEnvironment createClientEnvironment(const QString &socketName,
                                            const QString &controlPath,
                                            const QString &binaryDirectory) {
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.remove("DISPLAY");
  environment.remove("XAUTHORITY");
  environment.remove("QT_QPA_EGLFS_INTEGRATION");
  environment.insert("WAYLAND_DISPLAY", socketName);
  // Clients must never inherit the compositor's own EGLFS/KMS platform. The
  // ordered fallback list keeps a generic launch on Wayland while still letting
  // a toolkit whose client has no Wayland backend fall back to XWayland.
  environment.insert("QT_QPA_PLATFORM", "wayland;xcb");
  environment.insert("GDK_BACKEND", "wayland,x11");
  environment.insert("SDL_VIDEODRIVER", "wayland,x11");
  environment.insert("XDG_SESSION_TYPE", "wayland");
  environment.insert("XDG_CURRENT_DESKTOP", "LunaDash");
  environment.insert("XDG_SESSION_DESKTOP", "LunaDash");
  // Native Wayland is the primary client path. XWayland remains available for
  // applications that explicitly need it, but Chromium/Electron applications
  // should not be sent through the software-only XWayland backend by default.
  // WaylandCompositor::control("launch-command") reads the first variable and
  // adds Chromium's explicit Ozone flags; Electron understands the second one.
  environment.insert("LUNADASH_CHROMIUM_WAYLAND", "1");
  environment.insert("ELECTRON_OZONE_PLATFORM_HINT", "wayland");
  environment.insert("XMODIFIERS", "@im=fcitx");
  environment.insert("QT_IM_MODULE", "fcitx");
  environment.insert("QT_IM_MODULES", "wayland;fcitx;ibus");
  environment.insert("GTK_IM_MODULE", "fcitx");
  environment.insert("SDL_IM_MODULE", "fcitx");
  // kitty 0.48 still requests wl_data_device_manager v3 even though its
  // generated protocol supports only v1. Use XWayland for kitty until that
  // client-side protocol mismatch is fixed; the clipboard bridge keeps it
  // interoperable with native Wayland clients.
  if (environment.value("KITTY_DISABLE_WAYLAND").isEmpty())
    environment.insert("KITTY_DISABLE_WAYLAND", "1");
  auto assetDirectory = QStandardPaths::locate(
      QStandardPaths::GenericDataLocation, "lunadash/data/assets",
      QStandardPaths::LocateDirectory);
  if (assetDirectory.isEmpty())
    assetDirectory = QStringLiteral(LUDASH_ASSET_SOURCE_DIR);
  environment.insert("LUNADASH_ASSET_DIR", assetDirectory);
  environment.insert("LUNADASH_BIN_DIR", binaryDirectory);
  environment.insert("LUNADASH_CONTROL", controlPath);
  environment.insert("LUDASH_BIN_DIR", binaryDirectory);
  environment.insert("LUDASH_CONTROL", controlPath);
  const QString iconTheme = detectedIconTheme();
  if (!iconTheme.isEmpty())
    environment.insert("QS_ICON_THEME", iconTheme);
  environment.insert("QSG_RHI_BACKEND", "opengl");
  auto loggingRules = environment.value("QT_LOGGING_RULES");
  if (!loggingRules.isEmpty())
    loggingRules += ";";
  loggingRules += "quickshell.desktopentry.warning=false";
  environment.insert("QT_LOGGING_RULES", loggingRules);
  return environment;
}

bool publishClientEnvironment(const QProcessEnvironment &environment) {
  // Keep these variables local to the compositor and its children. A login
  // session publishes the display variables explicitly through
  // publishActivationEnvironment() on its own private bus; the host D-Bus or
  // systemd user manager of another desktop is never modified.
  for (const auto &name : environment.keys()) {
    const auto value = environment.value(name);
    qputenv(name.toUtf8(), value.toUtf8());
  }
  return true;
}

bool publishActivationEnvironment(const QProcessEnvironment &environment,
                                  QString *error) {
  // Only names a client needs in order to reach the compositor and its input
  // methods are published. DBUS_SESSION_BUS_ADDRESS and the runtime directory
  // already belong to the session that started the private bus.
  static const QStringList names = {QStringLiteral("WAYLAND_DISPLAY"),
                                    QStringLiteral("DISPLAY"),
                                    QStringLiteral("XAUTHORITY"),
                                    QStringLiteral("XDG_SESSION_TYPE"),
                                    QStringLiteral("XDG_CURRENT_DESKTOP"),
                                    QStringLiteral("XDG_SESSION_DESKTOP"),
                                    QStringLiteral("LUNADASH_CONTROL"),
                                    QStringLiteral("LUDASH_CONTROL"),
                                    QStringLiteral("QT_QPA_PLATFORM"),
                                    QStringLiteral("GDK_BACKEND"),
                                    QStringLiteral("SDL_VIDEODRIVER"),
                                    QStringLiteral("LUNADASH_CHROMIUM_WAYLAND"),
                                    QStringLiteral("ELECTRON_OZONE_PLATFORM_HINT"),
                                    QStringLiteral("XMODIFIERS"),
                                    QStringLiteral("QT_IM_MODULE"),
                                    QStringLiteral("QT_IM_MODULES"),
                                    QStringLiteral("GTK_IM_MODULE"),
                                    QStringLiteral("SDL_IM_MODULE")};
  const auto tool = QStandardPaths::findExecutable(
      QStringLiteral("dbus-update-activation-environment"));
  if (tool.isEmpty()) {
    if (error)
      *error = "dbus-update-activation-environment is not installed.";
    return false;
  }
  QStringList available;
  for (const auto &name : names)
    if (environment.contains(name))
      available.append(name);
  const auto run = [&](bool systemd, QString *output) {
    QStringList arguments;
    if (systemd)
      arguments.append(QStringLiteral("--systemd"));
    arguments += available;
    QProcess process;
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(tool, arguments);
    if (!process.waitForStarted(3000) || !process.waitForFinished(3000)) {
      process.kill();
      process.waitForFinished(1000);
      *output = "Timed out while publishing the activation environment.";
      return false;
    }
    if (process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
      *output = QString::fromLocal8Bit(process.readAll()).trimmed();
      return false;
    }
    return true;
  };
  QString message;
  if (run(true, &message))
    return true;
  // A session without a systemd user manager rejects --systemd after it has
  // already updated the bus, so retry without it before reporting a failure.
  QString fallback;
  if (run(false, &fallback))
    return true;
  if (error)
    *error = fallback.isEmpty() ? message : fallback;
  return false;
}
} // namespace LuDash
