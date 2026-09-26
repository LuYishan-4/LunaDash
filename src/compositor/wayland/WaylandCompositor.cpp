#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/window/WindowSwitcher.hpp"

#include "compositor/window/animation/WindowAnimation.hpp"
#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/input/Keyboard.hpp"
#include "compositor/ipc/ControlServer.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/session/SessionActions.hpp"
#include "compositor/session/SessionEnvironment.hpp"
#include "compositor/session/LaunchPolicy.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/settings/SettingsApi.hpp"
#include "core/settings/SettingsTarget.hpp"
#include "compositor/xwayland/XWaylandSupport.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/localization/Localization.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "core/templates/WaylandSlot.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/audio/AudioSettings.hpp"
#include "desktop/browser/Browser.hpp"
#include "desktop/display/BrightnessSettings.hpp"
#include "desktop/display/DdcBrightnessSettings.hpp"
#include "desktop/input/InputSettings.hpp"
#include "desktop/network/NetworkStatus.hpp"
#include "desktop/power/PowerSettings.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include "desktop/system/SystemStatus.hpp"
#include "desktop/system/SystemTools.hpp"
#include "desktop/system/UpdateChecker.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include "config/appearance/AppearancePresets.hpp"
#include "desktop/theme/ThemeSync.hpp"
#include "desktop/weather/WeatherStatus.hpp"
#include "shell/launcher/OrbitSettings.hpp"
#include "shell/modules/ShellModules.hpp"

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QPointer>
#include <QSettings>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <optional>

namespace LunaDash {
namespace {

using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;
using Templates::WaylandSlot;

bool isUtilityWindow(const QString &appId) {
  return appId == QLatin1String("io.github.bugaevc.wl-clipboard");
}

bool clientReady(const ClientWindow *client) {
  if (!client || !client->wlSurface)
    return false;
  if (client->x11)
    return client->xwayland != nullptr;
  return client->surface && client->surface->initialized &&
         client->toplevel != nullptr;
}

void setClientActivated(ClientWindow *client, bool active) {
  if (!client)
    return;
#if LUDASH_WLR_HAS_XWAYLAND
  if (client->x11 && client->xwayland) {
    wlr_xwayland_surface_activate(client->xwayland, active);
    return;
  }
#endif
  if (client->toplevel)
    wlr_xdg_toplevel_set_activated(client->toplevel, active);
}

void closeClient(ClientWindow *client) {
  if (!client)
    return;
#if LUDASH_WLR_HAS_XWAYLAND
  if (client->x11 && client->xwayland) {
    wlr_xwayland_surface_close(client->xwayland);
    return;
  }
#endif
  if (client->toplevel)
    wlr_xdg_toplevel_send_close(client->toplevel);
}

std::optional<int> textWindowId(const QString &text) {
  bool ok = false;
  const int id = text.toInt(&ok);
  if (!ok || id <= 0)
    return std::nullopt;
  return id;
}

bool groupWindowIds(const QString &text, int *window, int *target) {
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return false;
  const auto object = document.object();
  const auto left = object.value("window");
  const auto right = object.value("target");
  if (!left.isDouble() || !right.isDouble())
    return false;
  const qint64 a = left.toInteger();
  const qint64 b = right.toInteger();
  if (a <= 0 || b <= 0 || a > INT_MAX || b > INT_MAX || a == b)
    return false;
  *window = static_cast<int>(a);
  *target = static_cast<int>(b);
  return true;
}

} // namespace

WaylandCompositor::WaylandCompositor(const QByteArray &socket, bool fullscreen,
                                     bool startShell,
                                     const QString &rendererPreference)
    : d(std::make_unique<Impl>(this, socket, fullscreen, rendererPreference)) {
  windowTemplate_ = &windowTemplateForKey("tiling");
  windowLayout_ = createWindowLayout(*windowTemplate_);
  windowSwitcher_ = new WindowSwitcher(this);
  screenCapture_ = new ScreenCapture(this);
  connect(screenCapture_, &ScreenCapture::completed, this,
          [this](const QString &path, const QString &error) {
            if (!path.isEmpty()) {
              lastCapture_ = path;

              QString helper =
                  QStandardPaths::findExecutable("lunadash-clipboard-history");
              if (helper.isEmpty()) {
                const QString source =
                    QStringLiteral(LUDASH_SCRIPT_SOURCE_DIR) +
                    "/lunadash-clipboard-history";
                if (QFileInfo(source).isExecutable())
                  helper = source;
              }

              if (helper.isEmpty()) {
                qWarning("Screenshot saved, but clipboard helper is unavailable.");
              } else {
                auto *publisher = new QProcess(this);
                publisher->setProcessEnvironment(clientEnvironment_);
                publisher->setProcessChannelMode(QProcess::ForwardedChannels);
                connect(publisher, &QProcess::finished, this,
                        [publisher](int code, QProcess::ExitStatus status) {
                          if (status != QProcess::NormalExit || code != 0)
                            qWarning("Screenshot saved, but publishing it to the clipboard failed.");
                          publisher->deleteLater();
                        });
                connect(publisher, &QProcess::errorOccurred, this,
                        [publisher](QProcess::ProcessError) {
                          qWarning("Screenshot saved, but clipboard publisher could not start.");
                          publisher->deleteLater();
                        });
                publisher->start(
                    helper, {"publish-file", "image/png", path});
              }
            }
            captureError_ = error;
          });
  brightnessSettings_ = new BrightnessSettings(this);
  ddcBrightnessSettings_ = new DdcBrightnessSettings(this);
  shellModules_ = new ShellModules(this);
  systemStatus_ = new SystemStatus(this);
  weatherStatus_ = new WeatherStatus(this);
  QTimer::singleShot(0, this, [] {
    const auto path = QUrl(wallpaperImageUrl()).toLocalFile();
    if (!path.isEmpty()) refreshWallpaperPalette(path, wallpaperIsVideo(path));
  });
  audioSettings_ = new AudioSettings(this);
  powerSettings_ = new PowerSettings(this);
  sessionActions_ = new SessionActions(this);
  shortcutSettings_ = new ShortcutSettings;
  updateChecker_ = new UpdateChecker(this);
  networkStatus_ = new NetworkStatus(this);
  pluginManager_ = new PluginManager(this);
  pluginManager_->loadEnabled();
  windowLayout_->setPlacementFilter([this](auto workspace, auto area,
                                           const auto &placements) {
    return pluginWindowPlacements(*pluginManager_, *windowTemplate_, workspace,
                                  area, placements);
  });

  if (!d->initialize())
    return;

  connect(
      pluginManager_, &PluginManager::changed, this, [this] { arrange(); },
      Qt::QueuedConnection);
  windowAnimations_ = std::make_unique<SceneWindowAnimationTemplate>();
  const auto initialAppearance = desktopPreferences();
  windowAnimations_->configure(windowAnimationProfile(
      *pluginManager_, initialAppearance, *windowTemplate_));

  controlPath_ =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/" +
      QString::fromUtf8(socket) + "-control";
  windowSwitcher_->setChannelPath(controlPath_ + "-interaction.json");
  controlServer_ = new ControlServer(
      controlPath_,
      [this](const QJsonObject &request) { return control(request); }, this);

  clientEnvironment_ =
      createClientEnvironment(QString::fromUtf8(socket), controlPath_,
                              QCoreApplication::applicationDirPath());
  publishClientEnvironment(clientEnvironment_);

  xwayland_ = new XWaylandSupport(this);
  if (qEnvironmentVariableIntValue("LUDASH_DISABLE_XWAYLAND") != 1) {
    auto environment = clientEnvironment_;
    environment.insert(
        "XCURSOR_SIZE",
        QString::number(desktopPreferences().value("cursorSize").toInt()));
    if (!xwayland_->start(
            d->display, d->compositor, d->seat, environment,
            [this](wlr_xwayland_surface *surface) {
              if (d)
                d->addXWaylandSurface(surface);
            }))
      qWarning("XWayland/XWM could not be prepared; X11 clients are unavailable.");
  }

  QObject::connect(shellModules_, &ShellModules::changed, this,
                   [this] { arrange(); });

  const auto startSessionClients = [this, startShell] {
    if (!startShell || shuttingDown_ || testStopping_)
      return;

    if (qEnvironmentVariableIntValue("LUNADASH_PUBLISH_ACTIVATION_ENV") == 1) {
      const QString agent =
          QStandardPaths::findExecutable("lunadash-polkit-agent");
      if (!agent.isEmpty())
        spawn({}, agent, false);

      if (qEnvironmentVariableIntValue("LUNADASH_DISABLE_FCITX") != 1) {
        const QString fcitx = QStandardPaths::findExecutable("fcitx5");
        if (!fcitx.isEmpty())
          spawn({"--replace"}, fcitx, false);
      }
    }

    spawn({"--session"}, {}, false);

    if (setupComplete()) {
      QTimer::singleShot(1200, this, [this] {
        if (testStopping_ || shuttingDown_)
          return;
        for (const auto &app :
             desktopPreferences().value("startupApps").toArray())
          spawn({"--app", app.toString()});
      });
    }
  };

  // Portal backends are D-Bus activated. Do not start the shell/IME/apps until
  // the user bus and systemd manager know LunaDash's Wayland/display identity
  // and the portal frontend has been refreshed with that environment.
  publishSessionActivationEnvironment(startSessionClients);

  qInfo().noquote() << "LunaDash compositor backend: wlroots"
                    << "socket:" << socket << "input: libinput/xkbcommon"
                    << "QtWayland compositor: disabled";
}

WaylandCompositor::~WaylandCompositor() {
  shuttingDown_ = true;
  delete screenCapture_;
  screenCapture_ = nullptr;
  if (windowAnimations_)
    windowAnimations_->clear();
  if (xwayland_) {
    xwayland_->stop();
    delete xwayland_;
    xwayland_ = nullptr;
  }
  const auto childProcesses = processes_;
  processes_.clear();
  for (auto *process : childProcesses) {
    if (!process)
      continue;
    disconnect(process, nullptr, this, nullptr);
    if (process->state() == QProcess::NotRunning)
      continue;
    process->terminate();
    if (!process->waitForFinished(500)) {
      process->kill();
      process->waitForFinished(500);
    }
  }
  delete shortcutSettings_;
  shortcutSettings_ = nullptr;
  d.reset();
}

void WaylandCompositor::scheduleShellRestart() {
  if (shuttingDown_ || testStopping_ || logoutPending_ ||
      shellRestartScheduled_)
    return;

  const int exponent = std::min(shellRestartFailures_, 4);
  const int delayMs = std::min(4000, 250 * (1 << exponent));
  ++shellRestartFailures_;
  shellRestartScheduled_ = true;
  qWarning().noquote()
      << "LunaDash shell restart scheduled in" << delayMs << "ms";

  QTimer::singleShot(delayMs, this, [this] {
    shellRestartScheduled_ = false;
    if (shuttingDown_ || testStopping_ || logoutPending_)
      return;
    spawn({"--session", "--no-welcome"}, {}, false);
  });
}

QProcess *WaylandCompositor::spawn(const QStringList &arguments,
                                   const QString &program, bool required) {
  auto environment = clientEnvironment_;
  if (xwayland_)
    xwayland_->applyEnvironment(environment);
  else
    environment.remove("DISPLAY");

  auto *process = new QProcess(this);
  process->setProcessEnvironment(environment);
  process->setProcessChannelMode(QProcess::ForwardedChannels);
  connect(process, &QProcess::errorOccurred, this,
          [this, process, required](QProcess::ProcessError) {
            if (shuttingDown_)
              return;
            if (required)
              processFailure_ = true;
            qWarning().noquote()
                << "LunaDash child process error:" << process->errorString();
          });
  connect(process, &QProcess::finished, this,
          [this, process, required](int code, QProcess::ExitStatus status) {
            if (!shuttingDown_ &&
                (code != 0 || status != QProcess::NormalExit)) {
              if (required)
                processFailure_ = true;
              qWarning().noquote()
                  << "LunaDash child exited abnormally:" << process->program()
                  << "code" << code;
            }
            processes_.removeAll(process);
            process->deleteLater();
          });

  if (program.isEmpty() && arguments.contains("--session")) {
    connect(process, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus status) {
              if (shuttingDown_ || testStopping_ || logoutPending_)
                return;
              qWarning().noquote()
                  << "LunaDash shell exited; scheduling restart:"
                  << "code" << code << "status" << status;
              scheduleShellRestart();
            });
    connect(process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError) {
              if (!shuttingDown_ && !testStopping_ && !logoutPending_)
                scheduleShellRestart();
            });

    const QString sourceConfig =
        QStringLiteral(LUDASH_QML_SOURCE_DIR) + "/shell.qml";
    // Use assets next to the installed executable before any build checkout.
    // This also supports a staged/relocated installation in CI.
    QString config = QDir(QCoreApplication::applicationDirPath())
                         .absoluteFilePath("../share/lunadash/shell/shell.qml");
    if (!QFileInfo::exists(config))
      config = QFileInfo::exists(sourceConfig)
                   ? sourceConfig
                   : QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                            "lunadash/shell/shell.qml");
    if (config.isEmpty())
      config = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                      "ludash/shell/shell.qml");
    if (config.isEmpty()) {
      // Starting Quickshell with an empty --path only produces a silent,
      // windowless shell. Report the missing shell asset instead.
      if (required)
        processFailure_ = true;
      qWarning("LunaDash shell.qml was not found in any search location.");
      process->deleteLater();
      return nullptr;
    }
    const QString quickshell = QStandardPaths::findExecutable("quickshell");
    if (quickshell.isEmpty()) {
      if (required)
        processFailure_ = true;
      qWarning("Quickshell is unavailable.");
      process->deleteLater();
      return nullptr;
    }
    environment.insert("LUNADASH_WALLPAPER", wallpaperImageUrl());
    // Qt Multimedia's FFmpeg backend can use NVDEC through CUDA. Prefer it
    // only for the LunaDash shell on NVIDIA hosts and only when the user did
    // not choose a backend explicitly. Unsupported systems still use Qt's
    // normal software fallback.
    if (environment.value("QT_FFMPEG_DECODING_HW_DEVICE_TYPES").isEmpty() &&
        (QFileInfo::exists("/sys/module/nvidia") ||
         QFileInfo::exists("/proc/driver/nvidia/version")))
      environment.insert("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "cuda");
    // The shell draws its own theme. A blocking portal Settings query here
    // would prevent even the startup splash from reaching its first frame.
    environment.insert("QT_QPA_PLATFORMTHEME", "generic");
    environment.remove("GTK_USE_PORTAL");
    qInfo().noquote() << "LunaDash shell config:" << config;
    process->setProcessEnvironment(environment);
    process->start(quickshell, {"--path", config, "--no-color"});

    const QPointer<QProcess> shellGuard(process);
    QTimer::singleShot(10000, this, [this, shellGuard] {
      if (shellGuard && shellGuard->state() != QProcess::NotRunning)
        shellRestartFailures_ = 0;
    });
  } else {
    auto executable = program;
    if (executable.isEmpty()) {
      executable = QCoreApplication::applicationDirPath() + "/lunadash-desktop";
      if (!QFileInfo::exists(executable))
        executable = QCoreApplication::applicationDirPath() + "/ludash-desktop";
    }
    process->start(executable, arguments);
  }

  processes_ << process;
  return process;
}

bool WaylandCompositor::launchExternalCommand(QStringList command,
                                              QString *error, bool x11Helper) {
  if (command.isEmpty() || command.first().isEmpty()) {
    *error = "Expected an application command.";
    return false;
  }

  // Application/toolkit renderer selection belongs to the client. Do not
  // identify Chromium/Electron applications by executable names or rewrite
  // their ANGLE, Vulkan, GPU, Ozone or IME flags here. Besides being brittle,
  // scanning every command argument can misclassify URLs and document paths.
  //
  // XWayland is likewise opt-in for the launch request. Native Wayland clients
  // may still use DISPLAY from the prepared session environment when they
  // intentionally create an X11 helper, without LunaDash special-casing an
  // application such as Discord.
  if (x11Helper && (!xwayland_ || !xwayland_->startServer(error))) {
    if (error->isEmpty())
      *error =
          "This application's helper requires XWayland. Install and enable "
          "it, then restart LunaDash.";
    return false;
  }

  const QString executable = command.takeFirst();
  spawn(command, executable, false);
  return true;
}

bool WaylandCompositor::saveScreenshot(
    const QString &path, const std::function<void(bool)> &finished) {
  if (!d || !d->primaryOutput || !QFileInfo(path).isAbsolute() ||
      QFileInfo::exists(path) || screenCapture_->busy())
    return false;

  captureError_.clear();
  std::shared_ptr<QMetaObject::Connection> completion;
  if (finished) {
    completion = std::make_shared<QMetaObject::Connection>();
    *completion = connect(
        screenCapture_, &ScreenCapture::completed, this,
        [this, path, finished, completion](const QString &saved,
                                           const QString &error) {
          disconnect(*completion);
          finished(error.isEmpty() && saved == path &&
                   QFileInfo::exists(path));
        });
  }

  wlr_output_schedule_frame(d->primaryOutput);
  QString error;
  if (!screenCapture_->captureOutput(clientEnvironment_, path, &error)) {
    if (completion)
      disconnect(*completion);
    captureError_ = error;
    return false;
  }
  return true;
}

void WaylandCompositor::saveState(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return;
  file.write(QJsonDocument(state()).toJson(QJsonDocument::Indented));
}

bool WaylandCompositor::hasProcessFailure() const { return processFailure_; }

QJsonObject WaylandCompositor::currentWindowLayoutSettings() const {
  if (!windowTemplate_)
    return {};

  auto values = windowTemplate_->layoutSettingsDefaults;

  // Preserve legacy gap preferences until the user edits the new template API.
  if (windowTemplate_->key == "tiling") {
    const auto preferences = desktopPreferences();
    if (preferences.contains("gap"))
      values["gap"] = preferences.value("gap");
    const int legacyGap =
        configuredBuiltinSettings("window-layout").value("gap").toInt(-1);
    if (legacyGap >= 0)
      values["gap"] = legacyGap;
  }

  QSettings settings;
  const QString prefix = "windowLayout/" + windowTemplate_->key + "/";
  for (auto it = windowTemplate_->layoutSettingsSchema.begin();
       it != windowTemplate_->layoutSettingsSchema.end(); ++it)
    if (settings.contains(prefix + it.key()))
      values[it.key()] =
          Settings::fromStoredValue(it.value().toObject(), settings.value(prefix + it.key()));

  if (!validateWindowLayoutSettings(*windowTemplate_, values, nullptr))
    return windowTemplate_->layoutSettingsDefaults;
  return values;
}

bool WaylandCompositor::updateWindowLayoutSettings(const QJsonObject &changes,
                                                   QString *error) {
  if (!windowTemplate_) {
    if (error)
      *error = "No window layout template is active.";
    return false;
  }
  if (!validateWindowLayoutSettings(*windowTemplate_, changes, error))
    return false;

  auto values = currentWindowLayoutSettings();
  for (auto it = changes.begin(); it != changes.end(); ++it)
    values[it.key()] = it.value();
  if (!validateWindowLayoutSettings(*windowTemplate_, values, error))
    return false;

  QSettings settings;
  const QString prefix = "windowLayout/" + windowTemplate_->key + "/";
  for (auto it = changes.begin(); it != changes.end(); ++it)
    settings.setValue(prefix + it.key(), it.value().toVariant());
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error)
      *error = "Could not save window layout settings.";
    return false;
  }
  return true;
}

QRect WaylandCompositor::workArea() const {
  QRect area = d ? d->usableArea : QRect(0, 0, 1440, 900);
  const auto settings = currentWindowLayoutSettings();
  const int gap = settings.contains("gap")
                      ? settings.value("gap").toInt()
                      : desktopPreferences().value("gap").toInt();
  area.adjust(gap, gap, -gap, -gap);
  if (area.width() < 1)
    area.setWidth(1);
  if (area.height() < 1)
    area.setHeight(1);
  return area;
}

void WaylandCompositor::updateClientMetadata(ClientWindow *client) {
  if (!client)
    return;
#if LUDASH_WLR_HAS_XWAYLAND
  if (client->x11 && client->xwayland) {
    client->title = safeUtf8(client->xwayland->title);
    client->appId = safeUtf8(client->xwayland->class_
                                 ? client->xwayland->class_
                                 : client->xwayland->instance);
    client->processId = client->xwayland->pid;
    client->utility = client->xwayland->override_redirect;
    if (!client->initialRuleApplied)
      client->floating = client->utility || client->xwayland->parent ||
                         client->xwayland->modal;
    else if (client->xwayland->parent || client->xwayland->modal)
      client->floating = true;
  } else
#endif
  {
    if (!client->toplevel)
      return;
    client->title = safeUtf8(client->toplevel->title);
    client->appId = safeUtf8(client->toplevel->app_id);
    client->utility = isUtilityWindow(client->appId);
    if (!client->initialRuleApplied)
      client->floating = client->toplevel->parent != nullptr;
    else if (client->toplevel->parent)
      client->floating = true;
  }
  adoptScratchpad(client);
  client->iconName = client->utility
                         ? QString()
                         : windowIconName(client->appId, client->title);
}

void WaylandCompositor::configure(ClientWindow *client,
                                  const QRect &rectangle) {
  if (!clientReady(client) || !client->sceneTree)
    return;
  if (windowAnimations_)
    windowAnimations_->relayout(
        client->sceneTree, client->geometry, rectangle, d->animationLayer,
        client->mapped && client->sceneTree->node.enabled &&
            !client->manualResize && !d->pointerWindow);
  else
    wlr_scene_node_set_position(&client->sceneTree->node, rectangle.x(),
                                rectangle.y());
  client->geometry = rectangle;
  const QSize size(std::max(1, rectangle.width()),
                   std::max(1, rectangle.height()));

#if LUDASH_WLR_HAS_XWAYLAND
  if (client->x11 && client->xwayland) {
    const auto x = static_cast<int16_t>(
        std::clamp(rectangle.x(), -32768, 32767));
    const auto y = static_cast<int16_t>(
        std::clamp(rectangle.y(), -32768, 32767));
    const auto width = static_cast<uint16_t>(
        std::clamp(size.width(), 1, 65535));
    const auto height = static_cast<uint16_t>(
        std::clamp(size.height(), 1, 65535));
    const auto *surface = client->xwayland;
    if (surface->x != x || surface->y != y || surface->width != width ||
        surface->height != height)
      wlr_xwayland_surface_configure(client->xwayland, x, y, width, height);
    if (surface->maximized_horz != client->maximized ||
        surface->maximized_vert != client->maximized)
      WlrootsCompat::setXWaylandMaximized(client->xwayland, client->maximized);
    if (surface->fullscreen != client->fullscreen)
      wlr_xwayland_surface_set_fullscreen(client->xwayland, client->fullscreen);
    client->lastSize = size;
    return;
  }
#endif

  wlr_box geometry{};
#if WLR_VERSION_MINOR >= 19
  geometry = client->surface->geometry;
#else
  wlr_xdg_surface_get_geometry(client->surface, &geometry);
#endif
  wlr_box clip{geometry.x, geometry.y, rectangle.width(), rectangle.height()};
  if (client->sceneContent)
    wlr_scene_subsurface_tree_set_clip(&client->sceneContent->node,
                                       client->floating ? nullptr : &clip);
  if (client->lastSize != size) {
    client->lastSize = size;
    wlr_xdg_toplevel_set_size(client->toplevel, size.width(), size.height());
  }
  wlr_xdg_toplevel_set_maximized(client->toplevel, client->maximized);
  wlr_xdg_toplevel_set_fullscreen(client->toplevel, client->fullscreen);
}

void WaylandCompositor::arrange() {
  if (!d || !d->scene)
    return;
  const auto preferences = desktopPreferences();
  d->eyeCareActive = preferences.value("eyeCare").toBool();
  d->steadyWindowOpacity = d->eyeCareActive
      ? 1.0f
      : preferences.value("windowOpacity").toInt(90) / 100.0f;
  if (d->windowGlass) {
    const bool changed = d->windowGlass->configure(
        preferences.value("blur").toBool() && !d->eyeCareActive,
        preferences.value("blurRadius").toInt(),
        d->steadyWindowOpacity);
    if (changed)
      for (const auto *output : d->outputs)
        wlr_output_schedule_frame(output->output);
  }
  const auto templateKey = pluginManager_->windowTemplateKey();
  const auto &selectedTemplate = windowTemplateForKey(templateKey);
  const bool templateChanged =
      !windowTemplate_ || windowTemplate_->key != selectedTemplate.key;
  if (templateChanged) {
    if (d->pointerWindow)
      d->finishWindowPointer(false);
    windowTemplate_ = &selectedTemplate;
    windowLayout_ = createWindowLayout(*windowTemplate_);
    windowLayout_->setPlacementFilter(
        [this](auto workspace, auto area, const auto &placements) {
          return pluginWindowPlacements(*pluginManager_, *windowTemplate_,
                                        workspace, area, placements);
        });
  }
  if (windowAnimations_)
    windowAnimations_->configure(windowAnimationProfile(
        *pluginManager_, preferences, *windowTemplate_));
  const int count = preferences.value("workspaceCount").toInt();
  workspace_ = std::clamp(workspace_, 0, std::max(0, count - 1));
  const QRect area = workArea();
  const QSize output = d->outputSize();
  const QRect fullscreenArea(0, 0, std::max(1, output.width()),
                             std::max(1, output.height()));
  windowLayout_->configure(currentWindowLayoutSettings());

  ClientWindow *fullscreenClient = nullptr;
  for (const auto &candidate : clients_) {
    if (candidate->mapped && !candidate->minimized && candidate->fullscreen &&
        candidate->workspace == workspace_ && !candidate->utility) {
      fullscreenClient = candidate.get();
      break;
    }
  }

  for (const auto &client : clients_) {
    client->workspace = std::min(client->workspace, count - 1);
    const bool managed = windowUsesManagedLayout(*client);
    if (client->mapped) {
      int initialWidth = 0;
      int initialHeight = 0;
#if LUDASH_WLR_HAS_XWAYLAND
      if (client->x11 && client->xwayland) {
        initialWidth = client->xwayland->width;
        initialHeight = client->xwayland->height;
      } else
#endif
      if (client->surface) {
        wlr_box initialGeometry{};
#if WLR_VERSION_MINOR >= 19
        initialGeometry = client->surface->geometry;
#else
        wlr_xdg_surface_get_geometry(client->surface, &initialGeometry);
#endif
        initialWidth = initialGeometry.width;
        initialHeight = initialGeometry.height;
      }
      if (!client->preferredFloatingSize.isValid() &&
          initialWidth > 0 && initialHeight > 0)
        client->preferredFloatingSize =
            QSize(initialWidth, initialHeight);
    }
    if (managed) {
      const QSize preferred =
          windowTemplate_ && windowTemplate_->allowOverlap
              ? client->preferredFloatingSize
              : QSize();
      windowLayout_->insert(client->workspace, client->id, preferred);
      windowLayout_->moveToWorkspace(client->id, client->workspace);
      windowLayout_->setMinimized(client->id, client->minimized);
      if (client->maximized)
        windowLayout_->setMaximized(client->id, true);
    } else {
      windowLayout_->remove(client->id);
    }
  }

  if (templateChanged && focused_ && focused_->mapped && !focused_->minimized)
    windowLayout_->focus(focused_->id);
  QList<ClientWindow *> revealed;
  QHash<int, LayoutWindowId> maximizedWindows;
  for (const auto &client : clients_) {
    if (!maximizedWindows.contains(client->workspace))
      maximizedWindows.insert(
          client->workspace,
          windowLayout_->snapshot(client->workspace).maximizedWindow);
    const auto maximized = maximizedWindows.value(client->workspace);
    if (!client->floating && (client->mapped || client->layoutPending))
      client->maximized = maximized == static_cast<LayoutWindowId>(client->id);
    bool inMaximizedFamily = client->id == static_cast<int>(maximized);
    auto *parent = client->toplevel ? client->toplevel->parent : nullptr;
    for (std::size_t depth = 0; parent && depth < clients_.size(); ++depth) {
      const auto owner = std::find_if(
          clients_.begin(), clients_.end(),
          [parent](const auto &entry) { return entry->toplevel == parent; });
      if (owner != clients_.end() &&
          (*owner)->id == static_cast<int>(maximized))
        inMaximizedFamily = true;
      parent = parent->parent;
    }
    client->hiddenByMaximize =
        windowHiddenByMaximize(*windowTemplate_, maximized, inMaximizedFamily,
                               *client);

    bool inFullscreenFamily =
        fullscreenClient && client.get() == fullscreenClient;
    auto *fullscreenParent = client->toplevel ? client->toplevel->parent : nullptr;
    for (std::size_t depth = 0;
         fullscreenClient && fullscreenParent && depth < clients_.size();
         ++depth) {
      if (fullscreenParent == fullscreenClient->toplevel) {
        inFullscreenFamily = true;
        break;
      }
      fullscreenParent = fullscreenParent->parent;
    }

    const bool unmanagedX11 = client->x11 && client->utility;
    const bool visible =
        client->mapped && !client->minimized &&
        client->workspace == workspace_ &&
        (unmanagedX11 ||
         (!client->utility &&
          (fullscreenClient ? inFullscreenFamily : !client->hiddenByMaximize)));
    if (client->sceneTree) {
      auto *targetLayer =
          unmanagedX11 && d->overlayLayer
              ? d->overlayLayer
              : fullscreenClient && inFullscreenFamily && d->fullscreenLayer
                    ? d->fullscreenLayer
                    : d->normalLayer;
      if (client->sceneTree->node.parent != targetLayer)
        wlr_scene_node_reparent(&client->sceneTree->node, targetLayer);
      const bool wasVisible = client->sceneTree->node.enabled;
      if (!visible && wasVisible && client->mapped && windowAnimations_)
        windowAnimations_->close(client->sceneTree, d->animationLayer);
      if (visible && !wasVisible && client->geometry.isValid())
        revealed.append(client.get());
      if (!visible && windowAnimations_)
        windowAnimations_->cancel(client->sceneTree);
      wlr_scene_node_set_enabled(&client->sceneTree->node, visible);
    }

    if (!visible)
      continue;
    if (fullscreenClient && client.get() == fullscreenClient) {
      configure(client.get(), fullscreenArea);
      continue;
    }
    if (client->floating) {
      const QSize preferred = client->preferredFloatingSize.isValid()
                                  ? client->preferredFloatingSize
                                  : QSize(720, 500);
      const QSize bounded(std::min(preferred.width(), area.width()),
                          std::min(preferred.height(), area.height()));
      const QRect defaultGeometry(
          area.x() + (area.width() - bounded.width()) / 2,
          area.y() + (area.height() - bounded.height()) / 2, bounded.width(),
          bounded.height());
      auto floatingGeometry = client->manualGeometry.isValid()
                                  ? client->manualGeometry : defaultGeometry;
      floatingGeometry.setSize(floatingGeometry.size().boundedTo(area.size()));
      floatingGeometry.moveLeft(std::clamp(floatingGeometry.x(), area.left(), area.right() - floatingGeometry.width() + 1));
      floatingGeometry.moveTop(std::clamp(floatingGeometry.y(), area.top(), area.bottom() - floatingGeometry.height() + 1));
      configure(client.get(), client->maximized ? area : floatingGeometry);
    }
  }

  // Pending clients need their first size even when a rule sends them to a
  // different workspace, or an existing fullscreen window covers this one.
  QList<int> layoutWorkspaces{workspace_};
  for (const auto &client : clients_)
    if (client->layoutPending && !layoutWorkspaces.contains(client->workspace))
      layoutWorkspaces.append(client->workspace);
  QList<WindowPlacement> placements;
  for (const int workspace : layoutWorkspaces)
    placements.append(windowLayout_->presentation(workspace, area));
  if (windowTemplate_->key != pluginManager_->windowTemplateKey()) {
    arrange();
    return;
  }
  for (const auto &placement : placements) {
    auto found = std::find_if(
        clients_.begin(), clients_.end(), [&placement](const auto &client) {
          return client->id == static_cast<int>(placement.window);
        });
    if (found == clients_.end() || (*found)->minimized ||
        placement.hiddenByMaximize ||
        (!(*found)->layoutPending &&
         (fullscreenClient || (*found)->workspace != workspace_)))
      continue;
    configure(found->get(), placement.geometry);
  }

  if (windowAnimations_)
    for (auto *client : revealed)
      windowAnimations_->open(client->sceneTree, client->geometry);

  if (focused_ && focused_->sceneTree && focused_->mapped &&
      !focused_->minimized && focused_->workspace == workspace_)
    raiseWithDialogs(focused_);

  d->updateBackground();
  d->updateWindowCorners();
  publishWindowLayout();
}

void WaylandCompositor::raiseWithDialogs(ClientWindow *client) {
  if (!client || !client->sceneTree)
    return;
  wlr_scene_node_raise_to_top(&client->sceneTree->node);
  for (const auto &dialog : clients_) {
    if (!dialog->floating || !dialog->sceneTree ||
        !dialog->sceneTree->node.enabled || !dialog->toplevel)
      continue;
    auto *parent = dialog->toplevel->parent;
    for (std::size_t depth = 0; parent && depth < clients_.size(); ++depth) {
      if (parent == client->toplevel) {
        wlr_scene_node_raise_to_top(&dialog->sceneTree->node);
        break;
      }
      parent = parent->parent;
    }
  }
}

void WaylandCompositor::focus(ClientWindow *client) {
  if (!d || !client || !client->mapped || client->minimized ||
      client->utility || client->workspace != workspace_ ||
      !clientReady(client))
    return;

  ClientWindow *previous = focused_;
  if (focused_ && focused_ != client && clientReady(focused_))
    setClientActivated(focused_, false);

  const bool changed = focused_ != client;
  focused_ = client;

  if (!client->fullscreen && client->floating &&
      client->hiddenByMaximize) {
    for (const auto &other : clients_)
      if (!other->floating && other->workspace == client->workspace &&
          other->maximized)
        setMaximized(other.get(), false);
    arrange();
  }
  if (!client->fullscreen && windowTemplate_ &&
      !windowTemplate_->allowOverlap && !client->floating &&
      !client->maximized &&
      windowLayout_->snapshot(client->workspace).maximizedWindow)
    setMaximized(client, true);
  if (!client->floating)
    windowLayout_->focus(client->id);
  // Selecting a tiled window must also scroll its column into view. Avoid
  // relayout for repeated clicks in the same window (including popup grabs).
  if (changed && !client->floating)
    arrange();
  if (client->sceneTree)
    raiseWithDialogs(client);
  if (changed && windowAnimations_ && client->sceneTree)
    windowAnimations_->focus(client->sceneTree, client->geometry);
  if (changed)
    setClientActivated(client, true);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  if (changed) {
    if (previous && !previous->x11 && previous->nativeState)
      d->updateClientLegacyForeignToplevel(previous);
    if (!client->x11 && client->nativeState)
      d->updateClientLegacyForeignToplevel(client);
  }
#endif
  d->focusSurface(client->wlSurface);
  publishWindowLayout();
}

void WaylandCompositor::focusNext(int direction) {
  QList<ClientWindow *> visible;
  for (const auto &client : clients_)
    if (client->mapped && !client->minimized && !client->utility &&
        client->workspace == workspace_)
      visible << client.get();
  if (visible.isEmpty()) {
    ClientWindow *previous = focused_;
    if (previous && clientReady(previous))
      setClientActivated(previous, false);
    focused_ = nullptr;
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
    if (d)
      d->updateClientLegacyForeignToplevel(previous);
#endif
    if (d && d->seat)
      wlr_seat_keyboard_notify_clear_focus(d->seat);
    return;
  }
  const qsizetype index = visible.indexOf(focused_);
  focus(visible[(index + direction + visible.size()) % visible.size()]);
}

void WaylandCompositor::setMaximized(ClientWindow *client, bool maximized) {
  if (!client || client->maximized == maximized)
    return;
  if (!client->floating) {
    if (maximized) {
      for (const auto &other : clients_) {
        if (other.get() == client || other->workspace != client->workspace ||
            other->floating || !other->maximized)
          continue;
        other->maximized = false;
#if LUDASH_WLR_HAS_XWAYLAND
        if (other->x11 && other->xwayland)
          WlrootsCompat::setXWaylandMaximized(other->xwayland, false);
        else
#endif
        if (other->toplevel && other->surface && other->surface->initialized)
          wlr_xdg_toplevel_set_maximized(other->toplevel, false);
      }
    }
    windowLayout_->setMaximized(client->id, maximized);
  }
  client->maximized = maximized;
#if LUDASH_WLR_HAS_XWAYLAND
  if (client->x11 && client->xwayland)
    WlrootsCompat::setXWaylandMaximized(client->xwayland, maximized);
  else
#endif
  if (client->toplevel)
    wlr_xdg_toplevel_set_maximized(client->toplevel, maximized);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  if (d && !client->x11 && client->nativeState)
    d->updateClientLegacyForeignToplevel(client);
#endif
}

void WaylandCompositor::setFullscreen(ClientWindow *client, bool fullscreen) {
  if (!d || !client || client->fullscreen == fullscreen)
    return;

  if (fullscreen) {
    for (const auto &other : clients_) {
      if (other.get() == client || other->workspace != client->workspace ||
          !other->fullscreen)
        continue;
      other->fullscreen = false;
#if LUDASH_WLR_HAS_XWAYLAND
      if (other->x11 && other->xwayland)
        wlr_xwayland_surface_set_fullscreen(other->xwayland, false);
      else
#endif
      if (other->toplevel)
        wlr_xdg_toplevel_set_fullscreen(other->toplevel, false);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
      if (!other->x11 && other->nativeState)
        d->updateClientLegacyForeignToplevel(other.get());
#endif
    }
  }

  client->fullscreen = fullscreen;
#if LUDASH_WLR_HAS_XWAYLAND
  if (client->x11 && client->xwayland)
    wlr_xwayland_surface_set_fullscreen(client->xwayland, fullscreen);
  else
#endif
  if (client->toplevel)
    wlr_xdg_toplevel_set_fullscreen(client->toplevel, fullscreen);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  if (!client->x11 && client->nativeState)
    d->updateClientLegacyForeignToplevel(client);
#endif

  arrange();
  if (fullscreen)
    focus(client);
}

void WaylandCompositor::synchronizeWindowFocus() {
  const auto target = windowLayout_->snapshot(workspace_).focusedWindow;
  if (target) {
    auto found = std::find_if(clients_.begin(), clients_.end(),
                              [target](const auto &client) {
                                return client->id == static_cast<int>(target) &&
                                       client->mapped && !client->minimized;
                              });
    if (found != clients_.end()) {
      focus(found->get());
      return;
    }
  }
  focusNext(1);
}

void WaylandCompositor::removeClient(ClientWindow *client) {
  if (!client)
    return;
  if (scratchpadWindow_ == client->id)
    scratchpadWindow_ = 0;
  if (focused_ == client)
    focused_ = nullptr;
  windowSwitcher_->remove(client->id);
  windowLayout_->remove(client->id);
  std::erase_if(clients_,
                [client](const auto &entry) { return entry.get() == client; });
  arrange();
  synchronizeWindowFocus();
  if (logoutPending_ && clients_.empty())
    QCoreApplication::exit(hasProcessFailure() ? 2 : 0);
}

void WaylandCompositor::applyKeyboardConfiguration() {
  if (d)
    d->applyKeyboardConfig();
}

void WaylandCompositor::resendKeyboardModifiers() {
  if (!d || !d->seat)
    return;
  if (auto *keyboard = wlr_seat_get_keyboard(d->seat)) {
    wlr_seat_keyboard_notify_modifiers(d->seat, &keyboard->modifiers);
    ++modifierResends_;
  }
}

void WaylandCompositor::setLauncherVisible(bool visible, bool publish) {
  if (launcherVisible_ == visible)
    return;
  launcherVisible_ = visible;
  if (!publish)
    return;
  launcherSerial_ = launcherSerial_ >= 999999 ? 1 : launcherSerial_ + 1;
  windowSwitcher_->setLauncherState(launcherSerial_, launcherVisible_);
}


void WaylandCompositor::handleShortcut(const QString &action) {
  if (action.startsWith("workspace")) {
    bool ok = false;
    const int target =
        action.mid(QStringLiteral("workspace").size()).toInt(&ok);
    if (ok && target > 0 &&
        target <= desktopPreferences().value("workspaceCount").toInt()) {
      selectWorkspace(target - 1);
    }
    return;
  }

  if (action.startsWith("moveToWorkspace")) {
    bool ok = false;
    const int target =
        action.mid(QStringLiteral("moveToWorkspace").size()).toInt(&ok);
    if (ok && focused_ && target > 0 &&
        target <= desktopPreferences().value("workspaceCount").toInt()) {
      focused_->workspace = target - 1;
      windowLayout_->moveToWorkspace(focused_->id, focused_->workspace);
      arrange();
      synchronizeWindowFocus();
    }
    return;
  }

  if (focused_ && focused_->maximized &&
      (action == "reorderLeft" || action == "reorderRight" ||
       action == "groupLeft" || action == "groupRight" ||
       action == "expelWindow" || action == "centerColumn" ||
       action == "widenColumn" || action == "narrowColumn")) {
    setMaximized(focused_, false);
    arrange();
  }

  const auto performLayoutAction =
      [this](const QString &id, const QJsonObject &payload) {
        return windowTemplate_ &&
               performWindowLayoutAction(*windowLayout_, *windowTemplate_, id,
                                         payload);
      };

  if (action == "focusLeft")
    performLayoutAction(
        "focus-direction",
        {{"workspace", workspace_}, {"dx", -1}, {"dy", 0}});
  else if (action == "focusRight")
    performLayoutAction(
        "focus-direction",
        {{"workspace", workspace_}, {"dx", 1}, {"dy", 0}});
  else if (action == "focusUp")
    performLayoutAction(
        "focus-direction",
        {{"workspace", workspace_}, {"dx", 0}, {"dy", -1}});
  else if (action == "focusDown")
    performLayoutAction(
        "focus-direction",
        {{"workspace", workspace_}, {"dx", 0}, {"dy", 1}});
  else if (action == "reorderLeft" && focused_)
    performLayoutAction("reorder",
                        {{"window", focused_->id}, {"direction", -1}});
  else if (action == "reorderRight" && focused_)
    performLayoutAction("reorder",
                        {{"window", focused_->id}, {"direction", 1}});
  else if ((action == "groupLeft" || action == "groupRight") && focused_)
    performLayoutAction(
        "group-direction",
        {{"window", focused_->id},
         {"workspace", workspace_},
         {"dx", action == "groupLeft" ? -1 : 1},
         {"dy", 0}});
  else if (action == "expelWindow" && focused_)
    performLayoutAction("expel", {{"window", focused_->id}});
  else if (action == "centerColumn" && focused_) {
    const auto area = workArea();
    performLayoutAction(
        "center",
        {{"window", focused_->id},
         {"area", QJsonObject{{"x", area.x()},
                              {"y", area.y()},
                              {"width", area.width()},
                              {"height", area.height()}}}});
  } else if (action == "widenColumn" && focused_)
    performLayoutAction(
        "resize-width",
        {{"window", focused_->id},
         {"width", std::min(workArea().width(),
                            std::max(120, focused_->geometry.width() + 80))}});
  else if (action == "narrowColumn" && focused_)
    performLayoutAction(
        "resize-width",
        {{"window", focused_->id},
         {"width", std::max(120, focused_->geometry.width() - 80)}});
  else if (action == "maximizeWindow" && focused_) {
    setMaximized(focused_, !focused_->maximized);
  } else if (action == "toggleFullscreen" && focused_) {
    setFullscreen(focused_, !focused_->fullscreen);
  } else if ((action == "closeWindow" || action == "closeWindowAlternate") &&
             focused_)
    closeClient(focused_);
  else if (action == "minimizeWindow" && focused_) {
    focused_->minimized = true;
    windowLayout_->setMinimized(focused_->id, true);
  } else if (action == "launchTerminal" || action == "launchTerminalAlternate")
    control({{"method", "launch-default"}, {"value", "terminal"}});
  else if (action == "launchFiles")
    control({{"method", "launch-default"}, {"value", "files"}});
  else if (action == "toggleFloating" && focused_) {
    focused_->floating = !focused_->floating;
    focused_->maximized = false;
    focused_->manualGeometry = {};
    arrange();
    focus(focused_);
    return;
  } else if (action == "toggleScratchpad") {
    QString error;
    toggleScratchpad(&error);
    if (!error.isEmpty()) scratchpadError_ = error;
    return;
  } else if (action == "toggleEyeCare") {
    control({{"method", "eye-care"}, {"value", "toggle"}});
  } else if (action == "chooseWallpaper") {
    control({{"method", "choose-wallpaper"}});
  } else if (action == "randomWallpaper") {
    control({{"method", "wallpaper-random"}});
  } else if (action == "launchOrbit" || action == "openControlCenter" ||
             action == "openClipboard" || action == "openPowerMenu") {
    shellAction_ = action;
    shellActionSerial_ = shellActionSerial_ >= 999999 ? 1 : shellActionSerial_ + 1;
  } else if (action == "launchLauncher") {
    setLauncherVisible(!launcherVisible_);
    // The interaction channel is owner-only and publishes within one frame,
    // avoiding the shell status poll latency for an input shortcut.
    return;
  } else if (action == "screenshot") {
    captureScreen();
    // A repeated shortcut must leave keyboard focus on the active selector so
    // Escape can still cancel it.
    return;
  }

  arrange();
  synchronizeWindowFocus();
}

QJsonObject WaylandCompositor::state() const {
  QJsonArray entries;
  QJsonArray xwaylandUtilities;
  for (const auto &client : clients_) {
    if (client->utility) {
      // Keep notifications out of the task list while exposing their lifetime
      // separately for diagnostics, including the interval before destruction.
      if (client->x11)
        xwaylandUtilities.append(QJsonObject{{"id", client->id},
                                            {"mapped", client->mapped}});
      continue;
    }
    int bufferWidth = 0;
    int bufferHeight = 0;
    if (client->wlSurface) {
      bufferWidth = client->wlSurface->current.width;
      bufferHeight = client->wlSurface->current.height;
    }
    wlr_box contentGeometry{};
    if (client->surface) {
#if WLR_VERSION_MINOR >= 19
      contentGeometry = client->surface->geometry;
#else
      wlr_xdg_surface_get_geometry(client->surface, &contentGeometry);
#endif
    }
    const QRect frame = windowAnimations_
                            ? windowAnimations_->backend().visualGeometry(
                                  client->sceneTree, client->geometry)
                            : client->geometry;
    entries.append(QJsonObject{
        {"contentX", contentGeometry.x},
        {"contentY", contentGeometry.y},
        {"contentGeometryWidth", client->surface ? contentGeometry.width : bufferWidth},
        {"contentGeometryHeight", client->surface ? contentGeometry.height : bufferHeight},
        {"frameX", frame.x()},
        {"frameY", frame.y()},
        {"frameWidth", frame.width()},
        {"frameHeight", frame.height()},
        {"cornerRadius", d && d->windowCorners
                             ? d->windowCorners->radius(client->sceneTree) : 0},
        {"floating", client->floating},
        {"contentWidth", bufferWidth},
        {"contentHeight", bufferHeight},
        {"contentVisible",
         client->sceneTree ? client->sceneTree->node.enabled : false},
        {"contentPaintEnabled", true},
        {"bufferWidth", bufferWidth},
        {"bufferHeight", bufferHeight},
        {"id", client->id},
        {"title", client->title},
        {"appId", client->appId},
        {"icon", client->iconName},
        {"desktop", client->desktop},
        {"x11", client->x11},
        {"workspace", client->workspace},
        {"visible",
         client->sceneTree ? client->sceneTree->node.enabled : false},
        {"focused", client.get() == focused_},
        {"x", client->geometry.x()},
        {"y", client->geometry.y()},
        {"width", client->geometry.width()},
        {"height", client->geometry.height()},
        {"minimized", client->minimized},
        {"maximized", client->maximized},
        {"fullscreen", client->fullscreen},
        {"hiddenByMaximize", client->hiddenByMaximize},
        {"manualResize", client->manualResize},
        {"mapped", client->mapped}});
  }

  const auto tilingSnapshot = windowLayout_->snapshot(workspace_);
  QJsonArray tilingColumns;
  QJsonArray tilingGroups;
  QSet<int> emittedGroups;
  int fallbackGroup = 0;
  for (const auto &column : tilingSnapshot.columns) {
    const int group =
        column.metadata.value("group").toInt(fallbackGroup++);
    const int row = column.metadata.value("row").toInt(0);
    tilingColumns.append(
        QJsonObject{{"window", static_cast<qint64>(column.window)},
                    {"width", column.width},
                    {"minimized", column.minimized},
                    {"focused", column.focused},
                    {"columnIndex", group},
                    {"rowIndex", row},
                    {"x", column.geometry.x()},
                    {"y", column.geometry.y()},
                    {"height", column.geometry.height()}});
    if (emittedGroups.contains(group))
      continue;
    emittedGroups.insert(group);
    auto memberIds = column.metadata.value("members").toArray();
    if (memberIds.isEmpty())
      memberIds.append(static_cast<qint64>(column.window));
    QJsonArray members;
    bool groupFocused = false;
    for (const auto &idValue : memberIds) {
      const auto id = static_cast<LayoutWindowId>(idValue.toInteger());
      const auto found = std::find_if(
          clients_.cbegin(), clients_.cend(), [id](const auto &client) {
            return client->id == static_cast<int>(id);
          });
      if (found == clients_.cend())
        continue;
      const auto *member = found->get();
      const bool memberFocused = member == focused_ && !member->minimized;
      groupFocused = groupFocused || memberFocused;
      members.append(QJsonObject{{"window", static_cast<qint64>(id)},
                                 {"title", member->title},
                                 {"appId", member->appId},
                                 {"icon", member->iconName},
                                 {"focused", memberFocused},
                                 {"minimized", member->minimized}});
    }
    tilingGroups.append(QJsonObject{{"index", group},
                                    {"focused", groupFocused},
                                    {"width", column.width},
                                    {"members", members}});
  }

  auto preferences = desktopPreferences();
  const auto palette = appearancePalette(preferences);
  const auto extensions = pluginManager_->snapshot();
  preferences["plugins"] = extensions.value("installed");
  const auto layoutSettings = currentWindowLayoutSettings();
  const auto layoutTarget = Settings::target(
      "layout:" + windowTemplate_->key, "Window layout", "layout", "Windows",
      windowTemplate_->layoutSettingsSchema, layoutSettings);
  const auto moduleState = shellModules_->snapshot();
  auto xwaylandState = xwayland_ ? xwayland_->snapshot() : QJsonObject{};
  xwaylandState["utilitySurfaces"] = xwaylandUtilities;
  const auto language = selectedLanguage();
  const auto translations = languageDictionary(language);
  auto *physicalKeyboard = d ? d->preferredKeyboard() : nullptr;
  const auto lockEnabled = [physicalKeyboard](const char *name) {
    if (!physicalKeyboard || !physicalKeyboard->keymap)
      return false;
    const auto index =
        xkb_keymap_mod_get_index(physicalKeyboard->keymap, name);
    return index != XKB_MOD_INVALID &&
           (physicalKeyboard->modifiers.locked & (xkb_mod_mask_t{1} << index));
  };
  return {
      {"settingsApi", QJsonObject{{"version", 1},
          {"targets", settingsApiTargets(extensions, moduleState, layoutTarget)}}},
      {"layoutMode",
       windowTemplate_ && windowTemplate_->allowOverlap ? "stacking"
                                                        : "tiling"},
      {"windowLayout",
       QJsonObject{{"template", windowTemplate_ ? windowTemplate_->key
                                                : QString()},
                   {"allowOverlap",
                    windowTemplate_ ? windowTemplate_->allowOverlap : false},
                   {"schema", windowTemplate_
                                  ? windowTemplate_->layoutSettingsSchema
                                  : QJsonObject{}},
                   {"values", layoutSettings},
                   {"actions", windowTemplate_ ? windowTemplate_->layoutActions
                                               : QJsonObject{}}}},
      {"defaultApps", defaultApplications()},
      {"shellModules", moduleState},
      {"extensions", extensions},
      {"shellAnimationDuration",
       shellAnimationDuration(*pluginManager_,
                              preferences.value("animationDuration").toInt())},
      {"panelExtent",
       shellModules_->panelExtent(preferences.value("panelHeight").toInt())},
      {"workArea", QJsonObject{{"x", workArea().x()}, {"y", workArea().y()},
                                {"width", workArea().width()},
                                {"height", workArea().height()}}},
      {"panelEdge", shellModules_->panelEdge()},
      {"panelAtBottom", shellModules_->panelAtBottom()},
      {"audio", audioSettings_->snapshot()},
      {"power", powerSettings_->snapshot()},
      {"sessionActions", sessionActions_->snapshot()},
      {"shortcuts", shortcutSettings_->snapshot()},
      {"update", updateChecker_->snapshot()},
      {"settingsTools", systemSettingsTools()},
      {"display", d ? d->displaySnapshot() : QJsonObject{}},
      {"settingsSerial", settingsSerial_},
      {"settingsPage", settingsPage_},
      {"pickerSerial", pickerSerial_},
      {"launcherSerial", launcherSerial_},
      {"launcherOpen", launcherVisible_},
      {"input",
       QJsonObject{{"layout", keyboardLayoutPreference(preferences)},
                   {"repeatRate", keyboardRepeatRate()},
                   {"repeatDelay", keyboardRepeatDelay()},
                   {"modifierResends", modifierResends_},
                   {"keypadKeyForwards", keypadKeyForwards_},
                   {"lockedModifiers",
                    static_cast<qint64>(physicalKeyboard
                                            ? physicalKeyboard->modifiers.locked
                                            : 0)},
                   {"capsLock", lockEnabled("Lock")},
                   {"numLock", lockEnabled("NumLock")},
                   {"seatProtocolVersion",
                    WlrootsCompat::expectedSeatProtocolVersion()},
                   {"dataDeviceProtocolVersion", 3},
                   {"backend", "wlroots"},
                   {"qtInput", false},
                   {"inputMethodBridge", d ? d->inputBridgeReady() : false}}},
      {"blurReady", d && d->windowGlass && d->windowGlass->ready()},
      {"blurFailed", d && d->windowGlass && d->windowGlass->failed()},
      {"blurFrames", static_cast<qint64>(d && d->windowGlass
                                            ? d->windowGlass->frames() : 0)},
      {"blurError", d && d->windowGlass ? d->windowGlass->error() : QString()},
      {"activeAnimations",
       windowAnimations_ ? windowAnimations_->activeCount() : 0},
      {"xwayland", xwaylandState},
      {"screenCapture", QJsonObject{{"protocol", "zwlr_screencopy_manager_v1"},
                                    {"version", 3},
                                    {"frames", 0},
                                    {"lastCapture", lastCapture_},
                                    {"error", captureError_},
                                    {"busy", screenCapture_->busy()},
                                    {"phase", screenCapture_->phase()}}},
      {"activationEnvironment",
       QJsonObject{{"published", activationEnvironmentPublished_},
                   {"error", activationEnvironmentError_}}},
      {"brightness", brightnessSettings_->snapshot()},
      {"ddcBrightness", ddcBrightnessSettings_->snapshot()},
      {"appearance", preferences},
      {"palette", palette},
      {"applicationTheme", synchronizeApplicationTheme(preferences, palette)},
      {"appearancePresets", appearancePresets()},
      {"orbit", orbitSettings()},
      {"wallpapers", wallpaperSnapshot()},
      {"shellAction", shellAction_},
      {"shellActionSerial", shellActionSerial_},
      {"scratchpad", QJsonObject{{"window", scratchpadWindow_}, {"pending", scratchpadPending_}, {"error", scratchpadError_}}},
      {"setupComplete", setupComplete()},
      {"network", networkStatus_->snapshot()},
      {"system", systemStatus_->snapshot()},
      {"weather", weatherStatus_->snapshot()},
      {"wallpaperImage", wallpaperImageUrl()},
      {"workspace", workspace_},
      {"processFailure", processFailure_},
      {"clients", entries},
      {"pointerDrag", QJsonObject{{"window", d ? d->pointerWindow : 0},
                                  {"target", d ? d->pointerTarget : 0},
                                  {"edge", d ? d->pointerEdge : 0}}},
      {"tiling",
       QJsonObject{
           {"focusedWindow", static_cast<qint64>(tilingSnapshot.focusedWindow)},
           {"columns", tilingColumns},
           {"groups", tilingGroups}}},
      {"layerSurfaces", d ? d->mappedLayerCount() : 0},
      {"xdgPopupCount", d ? d->xdgPopups.size() : 0},
      {"language", language},
      {"translations", translations},
      {"wallpaper", QSettings().value("appearance/wallpaper", 0).toInt()},
      {"shutdown", testStopping_},
      {"graphicsApi", "wlroots"},
      {"shaderReady", d && d->renderer},
      {"graphicsFailed", processFailure_},
      {"graphicsMajor", 0},
      {"graphicsMinor", 0}};
}

QJsonObject WaylandCompositor::control(const QJsonObject &request) {
  const QString method = request.value("method").toString();
  const QString value = request.value("value").toString();
  if (method == "status")
    return state();

  bool numberValid = false;
  const int number = value.toInt(&numberValid);

  if (method == "workspace" && numberValid && number >= 0 &&
      number < desktopPreferences().value("workspaceCount").toInt()) {
    selectWorkspace(number);
  } else if (method == "language" && isSupportedLanguage(value)) {
    QSettings().setValue("appearance/language", value);
  } else if (method == "shortcut-capture" &&
             (value == "true" || value == "false")) {
    shortcutCapture_ = value == "true";
  } else if (method == "shortcuts") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!document.isObject() ||
        !shortcutSettings_->apply(document.object(), &error))
      return {{"error", error.isEmpty() ? "Expected shortcut JSON." : error}};
  } else if (method == "reset-shortcuts") {
    shortcutSettings_->reset();
  } else if (method == "capture") {
    if (!value.startsWith('/'))
      return {{"error", "Capture requires an absolute path."}};
    if (QFileInfo::exists(value))
      return {{"error", "Refusing to overwrite " + value}};
    if (!saveScreenshot(value))
      return {{"error", captureError_.isEmpty()
                            ? "Could not start capture for " + value
                            : captureError_}};
    return {{"path", value},
            {"pending", true},
            {"phase", screenCapture_->phase()}};
  } else if (method == "screenshot") {
    captureScreen();
    if (!captureError_.isEmpty())
      return {{"error", captureError_}};
    return {{"pending", screenCapture_->busy()},
            {"phase", screenCapture_->phase()}};
  } else if (method == "check-update") {
    updateChecker_->check();
  } else if (method == "send-key") {
    if (!d || !d->seat || !d->seat->keyboard_state.focused_surface)
      return {{"error", "No application has keyboard focus."}};
    auto *keyboard = wlr_seat_get_keyboard(d->seat);
    if (!keyboard || !keyboard->keymap)
      return {{"error", "No wlroots keyboard is available."}};
    const char *name = value == "copy"        ? "AC03"
                       : value == "paste"     ? "AB04"
                       : value == "cut"       ? "AB02"
                       : value == "selectAll" ? "AC01"
                                              : nullptr;
    if (!name)
      return {{"error", "Unknown key action."}};
    const xkb_keycode_t ctrl = xkb_keymap_key_by_name(keyboard->keymap, "LCTL");
    const xkb_keycode_t key = xkb_keymap_key_by_name(keyboard->keymap, name);
    if (!ctrl || !key)
      return {{"error", "Keymap does not contain the requested keys."}};
    const uint32_t now =
        static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0xffffffff);
    wlr_seat_keyboard_notify_key(d->seat, now, ctrl - 8,
                                 WL_KEYBOARD_KEY_STATE_PRESSED);
    wlr_seat_keyboard_notify_key(d->seat, now, key - 8,
                                 WL_KEYBOARD_KEY_STATE_PRESSED);
    wlr_seat_keyboard_notify_key(d->seat, now, key - 8,
                                 WL_KEYBOARD_KEY_STATE_RELEASED);
    wlr_seat_keyboard_notify_key(d->seat, now, ctrl - 8,
                                 WL_KEYBOARD_KEY_STATE_RELEASED);
  } else if (method == "open-settings") {
    static const QStringList pages{
        "general",      "appearance", "windows",   "shortcuts",   "display",
        "input",        "sound",      "network",   "bluetooth",   "power",
        "applications", "privacy",    "system",    "devices",     "about",
        "modules",      "plugins",    "dashboard", "input-method"};
    if (!value.isEmpty() && !pages.contains(value))
      return {{"error", "Unknown settings page."}};
    settingsPage_ = value.isEmpty() ? "general" : value;
    settingsSerial_ = (settingsSerial_ + 1) % 1000000;
  } else if (method == "settings-describe") {
    return state().value("settingsApi").toObject();
  } else if (method == "settings-update") {
    pluginManager_->refresh();
    arrange();
    if (value.toUtf8().size() > 24576)
      return {{"error", "Settings request exceeds 24 KiB."}};
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isObject() || !windowTemplate_)
      return {{"error", "Expected a settings update object."}};
    const auto layout = Settings::target("layout:" + windowTemplate_->key,
        "Window layout", "layout", "Windows", windowTemplate_->layoutSettingsSchema,
        currentWindowLayoutSettings());
    QString error;
    if (!updateSettingsApi(*pluginManager_, *shellModules_, layout, document.object(),
        [this](const auto &changes, QString *message) {
          return updateWindowLayoutSettings(changes, message);
        }, &error))
      return {{"error", error}, {"settingsTarget", document.object().value("target")}};
    arrange();
    auto result = state();
    result["settingsTarget"] = document.object().value("target");
    return result;
  } else if (method == "extension-save") {
    QString error;
    if (!saveExtensionConfiguration(value.toUtf8(), &error))
      return {{"error", error}};
    pluginManager_->refresh();
    arrange();
  } else if (method == "extension-install") {
    QString error;
    if (!pluginManager_->installFromStore(value, &error))
      return {{"error", error}};
    return {{"pending", true}, {"pluginId", value}};
  } else if (method == "extension-remove") {
    QString error;
    if (!pluginManager_->removeInstalledPlugin(value, &error))
      return {{"error", error}};
    return {{"removed", true}, {"pluginId", value}};
  } else if (method == "extension-error") {
    const auto object = QJsonDocument::fromJson(value.toUtf8()).object();
    pluginManager_->reportError(object.value("id").toString(),
                                object.value("error").toString());
  } else if (method == "module-validate" || method == "module-save" ||
             method == "module-reset") {
    QString error;
    bool ok = false;
    if (method == "module-validate")
      ok = shellModules_->validate(value.toUtf8(), &error);
    else if (method == "module-save")
      ok = shellModules_->apply(value.toUtf8(), &error);
    else if (method == "module-reset")
      ok = shellModules_->reset(&error);
    if (!ok)
      return {{"error", error.isEmpty() ? "Invalid module command." : error}};
  } else if (method == "module-error") {
    const auto object = QJsonDocument::fromJson(value.toUtf8()).object();
    shellModules_->reportError(object.value("id").toString(),
                               object.value("error").toString());
  } else if (method == "default-apps") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!document.isObject() ||
        !setDefaultApplications(document.object(), &error))
      return {{"error", error.isEmpty() ? "Expected a JSON object." : error}};
  } else if (method == "open-url") {
    QString error;
    auto command = Browser::commandForUrl(QUrl(value), &error);
    if (!error.isEmpty())
      return {{"error", error}};
    if (!launchExternalCommand(command, &error))
      return {{"error", error}};
  } else if (method == "launch-default" &&
             (value == "terminal" || value == "files" || value == "browser")) {
    QString error;
    auto command = defaultApplicationCommand(value, &error);
    if (!error.isEmpty())
      return {{"error", error}};
    if (command.isEmpty())
      return {{"error", "No application is configured for this role."}};
    if (!launchExternalCommand(command, &error))
      return {{"error", error}};
  } else if (method == "system-tool") {
    auto command = systemSettingsCommand(value);
    if (command.isEmpty())
      return {{"error", "This settings tool is unavailable."}};
    const QString program = command.takeFirst();
    spawn(command, program, false);
  } else if (method == "audio") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!document.isObject() ||
        !audioSettings_->apply(document.object(), &error))
      return {{"error", error.isEmpty() ? "Invalid audio setting." : error}};
  } else if (method == "network") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!document.isObject() ||
        !networkStatus_->execute(document.object(), &error))
      return {{"error", error.isEmpty() ? "Invalid network request." : error}};
  } else if (method == "power-profile") {
    QString error;
    if (!powerSettings_->apply(value, &error))
      return {{"error", error}};
  } else if (method == "session-action") {
    QString error;
    if (!isValidSessionAction(value) ||
        !sessionActions_->execute(value, &error))
      return {{"error", error.isEmpty() ? "Invalid session action." : error}};
  } else if (method == "brightness") {
    bool ok = false;
    const int percent = value.toInt(&ok);
    QString error;
    if (!ok || !brightnessSettings_->setPercent(percent, &error))
      return {{"error",
               error.isEmpty() ? "Brightness must be an integer." : error}};
  } else if (method == "ddc-brightness") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    const auto request = document.object();
    const auto percent = request.value("percent");
    QString error;
    if (!document.isObject() || !percent.isDouble() ||
        percent.toDouble() != percent.toInt(-1) ||
        !ddcBrightnessSettings_->setPercent(request.value("id").toString(),
                                            percent.toInt(-1), &error))
      return {{"error",
               error.isEmpty() ? "Invalid DDC/CI brightness request." : error}};
  } else if (method == "ddc-refresh") {
    ddcBrightnessSettings_->refresh();
  } else if (method == "display-configure") {
    const auto changes = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    if (!changes.isObject() || !d->configureDisplay(changes.object(), &error))
      return {{"error",
               error.isEmpty() ? "Invalid display configuration." : error}};
  } else if (method == "display-confirm") {
    d->confirmDisplay();
  } else if (method == "display-revert") {
    d->revertDisplay();
  } else if (method == "desktop-size") {
    QString error;
    if (!d || !d->resizePrimaryOutput(value, &error))
      return {{"error", error}};
  } else if (method == "reset-preferences") {
    QSettings settings;
    settings.remove("desktop");
    settings.sync();
    weatherStatus_->refresh();
    if (d) d->updateNightLight();
    applyKeyboardConfiguration();
    arrange();
  } else if (method == "window-layout-action") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isObject() || !windowTemplate_)
      return {{"error", "Expected a window layout action object."}};
    const auto object = document.object();
    const auto actionId = object.value("action").toString();
    if (!object.value("payload").isObject() || object.size() != 2)
      return {{"error", "Expected only action and payload fields."}};
    auto payload = object.value("payload").toObject();
    const auto action = windowTemplate_->layoutActions.value(actionId).toObject();
    if (action.value("requiresArea").toBool()) {
      if (payload.contains("area"))
        return {{"error", "Work area is supplied by the compositor, not the caller."}};
      const auto area = workArea();
      payload["area"] = QJsonObject{{"x", area.x()}, {"y", area.y()},
                                    {"width", area.width()}, {"height", area.height()}};
    }
    if (actionId.isEmpty() ||
        !performWindowLayoutAction(*windowLayout_, *windowTemplate_, actionId,
                                   payload))
      return {{"error", "Window layout action is unsupported or invalid."}};
    arrange();
    synchronizeWindowFocus();
    return state();
  } else if (method == "window-layout-settings") {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
    QString error;
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
      return {{"error", "Expected a JSON object of window layout settings."}};
    if (!updateWindowLayoutSettings(document.object(), &error))
      return {{"error", error}};
    arrange();
    return state();
  } else if (method == "launcher-visible") {
    if (value != "true" && value != "false")
      return {{"error", "launcher-visible expects true or false."}};
    setLauncherVisible(value == "true", false);
  } else if (method == "orbit-save" || method == "orbit-reset") {
    QString error;
    if (!(method == "orbit-save" ? saveOrbitSettings(value.toUtf8(), &error) : resetOrbitSettings(&error)))
      return {{"error", error}};
  } else if (method == "appearance-preset-save" || method == "appearance-preset-apply" || method == "appearance-preset-delete") {
    QString error;
    const bool ok = method == "appearance-preset-save" ? saveAppearancePreset(value, &error)
                    : method == "appearance-preset-apply" ? applyAppearancePreset(value, &error)
                    : deleteAppearancePreset(value, &error);
    if (!ok) return {{"error", error}};
    if (method == "appearance-preset-apply") {
      const auto updatedPreferences = desktopPreferences();
      synchronizeApplicationTheme(
          updatedPreferences, appearancePalette(updatedPreferences));
    }
    if (d) d->updateNightLight();
    arrange();
  } else if (method == "scratchpad") {
    QString error;
    if (!toggleScratchpad(&error)) return {{"error", error}};
  } else if (method == "eye-care") {
    if (value != "toggle" && value != "true" && value != "false")
      return {{"error", "Eye care expects toggle, true or false."}};
    QString error;
    const bool enabled = value == "toggle" ? !desktopPreferences().value("eyeCare").toBool() : value == "true";
    if (!updateDesktopPreferences({{"eyeCare", enabled}}, &error)) return {{"error", error}};
    if (d) d->updateNightLight();
    arrange();
  } else if (method == "wallpaper-random") {
    QString error;
    if (!selectRandomWallpaper(&error)) return {{"error", error}};
  } else if (method == "appearance") {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
    QString error;
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
      return {{"error", "Expected a JSON object of desktop preferences."}};
    if (!updateDesktopPreferences(document.object(), &error))
      return {{"error", error}};
    // Publish color-scheme immediately so portal-aware Chromium/Electron/Qt
    // clients and gsettings-aware GTK applications follow the desktop switch
    // without waiting for the next shell status poll.
    const auto updatedPreferences = desktopPreferences();
    synchronizeApplicationTheme(
        updatedPreferences, appearancePalette(updatedPreferences));
    applyKeyboardConfiguration();
    for (const auto &key : {"weatherEnabled", "weatherLatitude", "weatherLongitude", "weatherLocation"})
      if (document.object().contains(key)) { weatherStatus_->refresh(); break; }
    if (document.object().value("wallpaperColors").toBool()) {
      const auto path = QUrl(wallpaperImageUrl()).toLocalFile();
      refreshWallpaperPalette(path, wallpaperIsVideo(path));
    }
    if (d) {
      d->updateBackground();
      d->updateNightLight();
    }
    arrange();
  } else if (method == "launch-x11") {
    QString error;
    if (!xwayland_ || !xwayland_->launch(QProcess::splitCommand(value), &error))
      return {{"error", error.isEmpty() ? "XWayland is unavailable." : error}};
    for (const auto &client : clients_)
      updateClientMetadata(client.get());
    arrange();
  } else if (method == "launch-application") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isObject())
      return {{"error", "Expected an application launch object."}};
    const auto object = document.object();
    if (object.size() != 2 || !object.value("desktopId").isString() ||
        !object.value("command").isArray())
      return {{"error", "Application launch requires desktopId and command."}};
    const auto desktopId = object.value("desktopId").toString();
    if (desktopId.size() > 256)
      return {{"error", "Desktop application ID is too long."}};
    QStringList command;
    const auto values = object.value("command").toArray();
    if (values.isEmpty() || values.size() > 64)
      return {{"error", "Expected a bounded non-empty command array."}};
    for (const auto &entry : values) {
      if (!entry.isString() || entry.toString().isEmpty() ||
          entry.toString().size() > 1024)
        return {{"error", "Command arguments must be short strings."}};
      command << entry.toString();
    }
    const auto capabilities = resolveLaunchCapabilities(desktopId, command);
    QString error;
    if (!launchExternalCommand(command, &error, capabilities.x11Helper))
      return {{"error", error}};
  } else if (method == "launch-command" || method == "launch-with-x11") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isArray() || document.array().isEmpty())
      return {{"error", "Expected a non-empty command array."}};
    QStringList command;
    for (const auto &entry : document.array()) {
      if (!entry.isString() || entry.toString().size() > 1024)
        return {{"error", "Command arguments must be short strings."}};
      command << entry.toString();
    }
    QString error;
    if (!launchExternalCommand(command, &error, method == "launch-with-x11"))
      return {{"error", error}};
  } else if (method == "finish-setup") {
    setSetupComplete(true);
  } else if (method == "setup") {
    setSetupComplete(false);
  } else if (method == "configure-network") {
    auto program = QStandardPaths::findExecutable("nm-connection-editor");
    QStringList arguments;
    if (program.isEmpty() &&
        !QStandardPaths::findExecutable("nmtui").isEmpty()) {
      for (const auto &terminal : {"kitty", "konsole", "alacritty", "foot"}) {
        program = QStandardPaths::findExecutable(terminal);
        if (!program.isEmpty()) {
          arguments = {"-e", "nmtui"};
          break;
        }
      }
    }
    if (program.isEmpty())
      return {{"error", "No network configuration utility is installed."}};
    spawn(arguments, program, false);
  } else if (method == "choose-wallpaper") {
    // Super+W is a standalone wallpaper-gallery toggle. Refresh discovery only
    // when the user opens the gallery; background status polls reuse the cache
    // and never rescan a large Pictures/Wallpapers tree on a timer.
    QString refreshError;
    if (!refreshWallpaperLibrary(&refreshError))
      return {{"error", refreshError}};
    pickerSerial_ = (pickerSerial_ + 1) % 1000000;
  } else if (method == "wallpaper-image") {
    QString error;
    const QUrl url(value);
    if (!setWallpaperImage(url.isLocalFile() ? url.toLocalFile() : value,
                           &error))
      return {{"error", error}};
  } else if (method == "wallpaper-default") {
    resetWallpaperImage();
  } else if (method == "wallpaper" && numberValid && number >= 0 &&
             number <= 1) {
    QSettings().setValue("appearance/wallpaper", number);
    QSettings().setValue("appearance/wallpaperMode", "shader");
    if (d)
      d->updateBackground();
  } else if (method == "group-window") {
    int window = 0;
    int target = 0;
    if (!groupWindowIds(value, &window, &target))
      return {{"error", "Expected bounded JSON window and target IDs."}};
    if (!windowTemplate_ ||
        !performWindowLayoutAction(*windowLayout_, *windowTemplate_,
                                   "group-with",
                                   {{"window", window}, {"target", target}}))
      return {{"error", "Windows cannot be grouped."}};
    arrange();
  } else if (method == "expel-window") {
    const auto window = textWindowId(value);
    if (!window || !windowTemplate_ ||
        !performWindowLayoutAction(*windowLayout_, *windowTemplate_, "expel",
                                   {{"window", *window}}))
      return {{"error", "Window layout does not support expelling this window."}};
    arrange();
    synchronizeWindowFocus();
  } else if (method == "quit") {
    if (value != "confirm")
      return {{"error", "Logout requires explicit confirmation."}};
    qInfo("LunaDash logout requested through the control channel.");
    QTimer::singleShot(0, this, &WaylandCompositor::requestShutdown);
  } else if (method == "switch-window") {
    const auto id = textWindowId(value);
    if (!id || !windowSwitcher_->select(*id))
      return {{"error", "Unknown selection"}};
  } else if (method == "switch-step") {
    windowSwitcher_->step(value == "previous" ? -1 : 1);
  } else if (method == "switch-accept" || method == "switch-cancel") {
    finishWindowSwitch(method == "switch-accept");
  } else if (method == "activate-window") {
    const auto id = textWindowId(value);
    if (!id)
      return {{"error", "Expected a window ID"}};
    activateTask(*id);
  } else if (method == "focus" || method == "close" || method == "minimize") {
    const auto id = textWindowId(value);
    if (!id)
      return {{"error", "Window command requires a positive integer ID."}};
    for (const auto &client : clients_) {
      if (client->id != *id)
        continue;
      if (method == "close") {
        closeClient(client.get());
      } else if (method == "minimize") {
        client->minimized = true;
#if LUDASH_WLR_HAS_XWAYLAND
        if (client->x11 && client->xwayland)
          wlr_xwayland_surface_set_minimized(client->xwayland, true);
#endif
        windowLayout_->setMinimized(client->id, true);
        arrange();
        synchronizeWindowFocus();
      } else {
        workspace_ = client->workspace;
        client->minimized = false;
        windowLayout_->setMinimized(client->id, false);
        windowLayout_->focus(client->id);
        arrange();
        focus(client.get());
      }
      return state();
    }
    return {{"error", "Unknown window"}};
  } else {
    return {{"error", "Invalid command or value"}};
  }

  return state();
}

void WaylandCompositor::publishSessionActivationEnvironment(
    const std::function<void()> &ready) {
  if (qEnvironmentVariableIntValue("LUNADASH_PUBLISH_ACTIVATION_ENV") != 1) {
    if (ready)
      ready();
    return;
  }
  auto environment = clientEnvironment_;
  if (xwayland_)
    xwayland_->applyEnvironment(environment);
  publishActivationEnvironment(
      environment, this,
      [this, ready](bool ok, const QString &error) {
        activationEnvironmentPublished_ = ok;
        activationEnvironmentError_ = error;
        if (!ok) {
          if (ready)
            ready();
          return;
        }
        refreshScreencastPortalServices(clientEnvironment_, this, ready);
      });
}

QString WaylandCompositor::nextCapturePath() const {
  auto directory =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  if (directory.isEmpty())
    directory = QDir::homePath() + "/Pictures";
  directory += "/Screenshots";
  if (!QDir().mkpath(directory))
    return {};
  return directory + "/LunaDash-" +
         QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz") + ".png";
}

void WaylandCompositor::captureScreen() {
  if (screenCapture_->busy())
    return;
  captureError_.clear();
  const QString path = nextCapturePath();
  if (path.isEmpty()) {
    captureError_ = "Could not create the screenshot directory.";
    return;
  }
  screenCapture_->selectRegion(clientEnvironment_, path, &captureError_);
}

void WaylandCompositor::closeTestSession(
    const std::function<void(bool)> &finished) {
  testStopping_ = true;
  for (const auto &client : clients_)
    closeClient(client.get());

  auto *timer = new QTimer(this);
  auto elapsed = std::make_shared<int>(0);
  connect(timer, &QTimer::timeout, this, [this, timer, elapsed, finished] {
    *elapsed += 50;
    if (clients_.empty() && xwayland_)
      xwayland_->stop();
    const bool xwaylandDone = !xwayland_ || xwayland_->stopped();
    const bool processesDone = std::all_of(
        processes_.cbegin(), processes_.cend(), [](const QProcess *process) {
          return !process || process->state() == QProcess::NotRunning;
        });
    if ((clients_.empty() && processesDone && xwaylandDone) ||
        *elapsed >= 5000) {
      timer->stop();
      timer->deleteLater();
      finished(clients_.empty() && processesDone && xwaylandDone &&
               !processFailure_);
    }
  });
  timer->start(50);
}

void WaylandCompositor::requestShutdown() {
  logoutPending_ = true;
  bool applications = false;
  for (const auto &client : clients_) {
    if (client->utility)
      continue;
    applications = true;
    closeClient(client.get());
  }
  if (!applications)
    QCoreApplication::exit(hasProcessFailure() ? 2 : 0);
}

} // namespace LunaDash
