#include <LuDash/session_environment/SessionEnvironment.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDebug>
#include <QMap>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace LuDash {
namespace {
// The shell inherits LunaDah's own desktop identity, so Qt cannot infer the
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
  environment.remove("QT_QPA_EGLFS_INTEGRATION");
  environment.insert("WAYLAND_DISPLAY", socketName);
  environment.insert("QT_QPA_PLATFORM", "wayland");
  environment.insert("XDG_SESSION_TYPE", "wayland");
  environment.insert("XDG_CURRENT_DESKTOP", "LunaDah");
  environment.insert("XDG_SESSION_DESKTOP", "LunaDah");
  environment.insert("XMODIFIERS", "@im=fcitx");
  environment.insert("QT_IM_MODULE", "fcitx");
  environment.insert("QT_IM_MODULES", "wayland;fcitx;ibus");
  environment.insert("GTK_IM_MODULE", "fcitx");
  environment.insert("SDL_IM_MODULE", "fcitx");
  auto assetDirectory = QStandardPaths::locate(
      QStandardPaths::GenericDataLocation, "lunadah/data/assets",
      QStandardPaths::LocateDirectory);
  if (assetDirectory.isEmpty())
    assetDirectory = QStringLiteral(LUDASH_ASSET_SOURCE_DIR);
  environment.insert("LUNADAH_ASSET_DIR", assetDirectory);
  environment.insert("LUNADAH_BIN_DIR", binaryDirectory);
  environment.insert("LUNADAH_CONTROL", controlPath);
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
  qDBusRegisterMetaType<QMap<QString, QString>>();
  QMap<QString, QString> activation;
  QStringList systemd;
  for (const auto &name : environment.keys()) {
    const auto value = environment.value(name);
    qputenv(name.toUtf8(), value.toUtf8());
    if (name == "WAYLAND_DISPLAY" || name == "QT_QPA_PLATFORM" ||
        name == "XDG_SESSION_TYPE" || name == "XDG_CURRENT_DESKTOP" ||
        name == "XDG_SESSION_DESKTOP" || name == "XMODIFIERS" ||
        name == "QT_IM_MODULE" || name == "QT_IM_MODULES" ||
        name == "GTK_IM_MODULE" || name == "SDL_IM_MODULE" ||
        name == "LUNADAH_ASSET_DIR" || name == "LUNADAH_BIN_DIR" ||
        name == "LUNADAH_CONTROL" || name == "LUDASH_BIN_DIR" ||
        name == "LUDASH_CONTROL") {
      activation.insert(name, value);
      systemd.append(name + "=" + value);
    }
  }

  bool published = true;
  if (QDBusConnection::sessionBus().isConnected()) {
    auto dbus = QDBusMessage::createMethodCall(
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "UpdateActivationEnvironment");
    dbus << QVariant::fromValue(activation);
    const auto dbusReply =
        QDBusConnection::sessionBus().call(dbus, QDBus::Block, 2000);
    if (dbusReply.type() == QDBusMessage::ErrorMessage) {
      published = false;
      qWarning().noquote()
          << "LunaDah could not update the D-Bus activation environment:"
          << dbusReply.errorMessage();
    }

    auto manager = QDBusMessage::createMethodCall(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "SetEnvironment");
    manager << systemd;
    QDBusConnection::sessionBus().asyncCall(manager);
  }
  return published;
}
} // namespace LuDash
