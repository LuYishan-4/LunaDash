#include "compositor/session/SessionEnvironment.hpp"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <memory>

namespace LunaDash {
namespace {
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
    if (!QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                "icons/" + name + "/index.theme")
             .isEmpty())
      return name;
  }
  return {};
}

void applyProxyEnvironment(QProcessEnvironment &environment) {
  static const QStringList proxyVariables = {
      "http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY",
      "all_proxy",  "ALL_PROXY",  "no_proxy",    "NO_PROXY"};
  for (const auto &name : proxyVariables)
    environment.remove(name);

  QSettings settings;
  if (!settings.value("desktop/proxyEnabled", false).toBool())
    return;

  const QString http = settings.value("desktop/proxyHttp").toString().trimmed();
  const QString https =
      settings.value("desktop/proxyHttps").toString().trimmed();
  const QString socks =
      settings.value("desktop/proxySocks").toString().trimmed();
  const QString bypass =
      settings.value("desktop/proxyBypass").toString().trimmed();
  if (!http.isEmpty()) {
    environment.insert("http_proxy", http);
    environment.insert("HTTP_PROXY", http);
  }
  if (!https.isEmpty()) {
    environment.insert("https_proxy", https);
    environment.insert("HTTPS_PROXY", https);
  }
  if (!socks.isEmpty()) {
    environment.insert("all_proxy", socks);
    environment.insert("ALL_PROXY", socks);
  }
  if (!bypass.isEmpty()) {
    environment.insert("no_proxy", bypass);
    environment.insert("NO_PROXY", bypass);
  }
}
} // namespace

QProcessEnvironment createClientEnvironment(const QString &socketName,
                                            const QString &controlPath,
                                            const QString &binaryDirectory) {
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.remove("DISPLAY");
  environment.remove("XAUTHORITY");
  environment.remove("QT_QPA_EGLFS_INTEGRATION");
  environment.remove("KITTY_DISABLE_WAYLAND");
  environment.insert("WAYLAND_DISPLAY", socketName);
  environment.insert("QT_QPA_PLATFORM", "wayland;xcb");
  environment.insert("GDK_BACKEND", "wayland,x11");
  environment.insert("SDL_VIDEODRIVER", "wayland,x11");
  environment.insert("GLFW_PLATFORM", "wayland");
  environment.insert("XDG_SESSION_TYPE", "wayland");
  environment.insert("XDG_CURRENT_DESKTOP", "LunaDash");
  environment.insert("XDG_SESSION_DESKTOP", "LunaDash");
  environment.insert("LUNADASH_CHROMIUM_WAYLAND", "1");
  environment.insert("ELECTRON_OZONE_PLATFORM_HINT", "wayland");
  environment.insert("XMODIFIERS", "@im=fcitx");
  environment.insert("QT_IM_MODULE", "fcitx");
  // The compositor-side bridge is implemented by wlroots
  // text-input-v3/input-method-v2. Toolkit input modules remain client-side:
  // Fcitx can use the native Wayland protocols without any Qt compositor path.
  environment.insert("QT_IM_MODULES", "fcitx;wayland");
  environment.insert("GTK_IM_MODULE", "fcitx");
  environment.insert("SDL_IM_MODULE", "fcitx");
  environment.insert("INPUT_METHOD", "fcitx");
  applyProxyEnvironment(environment);
  auto assetDirectory = QStandardPaths::locate(
      QStandardPaths::GenericDataLocation, "lunadash/data/assets",
      QStandardPaths::LocateDirectory);
  if (assetDirectory.isEmpty())
    assetDirectory = QStringLiteral(LUDASH_ASSET_SOURCE_DIR);
  environment.insert("LUNADASH_ASSET_DIR", assetDirectory);

  auto wallpaperDirectory = QStandardPaths::locate(
      QStandardPaths::GenericDataLocation, "ludash/wallpapers",
      QStandardPaths::LocateDirectory);
  if (wallpaperDirectory.isEmpty())
    wallpaperDirectory =
        QFileInfo(assetDirectory).absoluteDir().filePath("wallpapers");
  environment.insert("LUNADASH_WALLPAPER_DIR", wallpaperDirectory);

  environment.insert("LUNADASH_BIN_DIR", binaryDirectory);
  environment.insert("LUNADASH_CONTROL", controlPath);
  environment.insert("LUDASH_BIN_DIR", binaryDirectory);
  environment.insert("LUDASH_CONTROL", controlPath);
  const QString iconTheme = detectedIconTheme();
  if (!iconTheme.isEmpty())
    environment.insert("QS_ICON_THEME", iconTheme);
  // Respect an explicitly selected client renderer (for example the
  // Qt Quick software adaptation in headless CI and remote sessions). OpenGL
  // remains the normal default when the environment does not choose one.
  if (environment.value("QT_QUICK_BACKEND").isEmpty() &&
      environment.value("QSG_RHI_BACKEND").isEmpty())
    environment.insert("QSG_RHI_BACKEND", "opengl");
  auto loggingRules = environment.value("QT_LOGGING_RULES");
  if (!loggingRules.isEmpty())
    loggingRules += ";";
  loggingRules += "quickshell.desktopentry.warning=false";
  environment.insert("QT_LOGGING_RULES", loggingRules);
  return environment;
}

bool publishClientEnvironment(const QProcessEnvironment &environment) {
  for (const auto &name : environment.keys())
    qputenv(name.toUtf8(), environment.value(name).toUtf8());
  return true;
}

void refreshScreencastPortalServices(
    const QProcessEnvironment &environment, QObject *owner) {
  const QString systemctl =
      QStandardPaths::findExecutable(QStringLiteral("systemctl"));
  if (systemctl.isEmpty())
    return;

  auto *process = new QProcess(owner);
  process->setProcessEnvironment(environment);
  process->setProcessChannelMode(QProcess::MergedChannels);
  QObject::connect(
      process, &QProcess::finished, owner,
      [process](int code, QProcess::ExitStatus status) {
        if (code != 0 || status != QProcess::NormalExit) {
          const QString diagnostic =
              QString::fromLocal8Bit(process->readAll()).trimmed().left(1024);
          if (!diagnostic.isEmpty())
            qWarning().noquote()
                << "LunaDash portal refresh:" << diagnostic;
        }
        process->deleteLater();
      });
  QObject::connect(
      process, &QProcess::errorOccurred, owner,
      [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
          process->deleteLater();
      });
  process->start(
      systemctl,
      {QStringLiteral("--user"), QStringLiteral("try-restart"),
       QStringLiteral("xdg-desktop-portal-wlr.service"),
       QStringLiteral("xdg-desktop-portal.service")});
}

void publishActivationEnvironment(
    const QProcessEnvironment &environment, QObject *owner,
    std::function<void(bool, const QString &)> completion) {
  static const QStringList names = {
      QStringLiteral("WAYLAND_DISPLAY"),
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
      QStringLiteral("GLFW_PLATFORM"),
      QStringLiteral("LUNADASH_CHROMIUM_WAYLAND"),
      QStringLiteral("ELECTRON_OZONE_PLATFORM_HINT"),
      QStringLiteral("XMODIFIERS"),
      QStringLiteral("QT_IM_MODULE"),
      QStringLiteral("QT_IM_MODULES"),
      QStringLiteral("GTK_IM_MODULE"),
      QStringLiteral("SDL_IM_MODULE"),
      QStringLiteral("INPUT_METHOD"),
      QStringLiteral("http_proxy"),
      QStringLiteral("HTTP_PROXY"),
      QStringLiteral("https_proxy"),
      QStringLiteral("HTTPS_PROXY"),
      QStringLiteral("all_proxy"),
      QStringLiteral("ALL_PROXY"),
      QStringLiteral("no_proxy"),
      QStringLiteral("NO_PROXY")};
  const auto tool = QStandardPaths::findExecutable(
      QStringLiteral("dbus-update-activation-environment"));
  if (tool.isEmpty()) {
    completion(false, "dbus-update-activation-environment is not installed.");
    return;
  }
  QStringList available;
  for (const auto &name : names)
    if (environment.contains(name))
      available.append(name);
  auto *process = new QProcess(owner);
  auto *timeout = new QTimer(process);
  timeout->setSingleShot(true);
  timeout->setInterval(3000);
  process->setProcessEnvironment(environment);
  process->setProcessChannelMode(QProcess::MergedChannels);
  const auto fallback = std::make_shared<bool>(false);
  QObject::connect(timeout, &QTimer::timeout, process,
                   [process] { process->kill(); });
  QObject::connect(
      process, &QProcess::finished, owner,
      [process, timeout, fallback, available, tool,
       completion](int code, QProcess::ExitStatus status) {
        timeout->stop();
        const QString message =
            QString::fromLocal8Bit(process->readAll()).trimmed().left(1024);
        if (code == 0 && status == QProcess::NormalExit) {
          completion(true, {});
          process->deleteLater();
        } else if (!*fallback) {
          *fallback = true;
          process->start(tool, available);
          timeout->start();
        } else {
          completion(
              false,
              message.isEmpty()
                  ? "Activation environment publication failed or timed out."
                  : message);
          process->deleteLater();
        }
      });
  QObject::connect(
      process, &QProcess::errorOccurred, owner,
      [process, timeout, completion](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
          return;
        timeout->stop();
        completion(false, process->errorString());
        process->deleteLater();
      });
  process->start(tool, QStringList{"--systemd"} + available);
  timeout->start();
}
} // namespace LunaDash
