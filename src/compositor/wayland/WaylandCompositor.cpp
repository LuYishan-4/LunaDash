#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/renderer/selection/RenderSelection.h"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "config/BuildConfig.hpp"
#include "shell/runtime/ShellRenderer.hpp"

#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/input/Keyboard.hpp"
#include "compositor/ipc/ControlServer.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/session/LaunchPolicy.hpp"
#include "compositor/session/SessionActions.hpp"
#include "compositor/session/SessionEnvironment.hpp"
#include "compositor/session/SessionServices.hpp"
#include "compositor/settings/SettingsApi.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/animation/WindowAnimation.hpp"
#include "compositor/xwayland/XWaylandSupport.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include "config/appearance/AppearancePresets.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/localization/Localization.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "core/settings/SettingsTarget.hpp"
#include "core/templates/WaylandSlot.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/audio/AudioSettings.hpp"
#include "desktop/display/BrightnessSettings.hpp"
#include "desktop/display/DdcBrightnessSettings.hpp"
#include "desktop/input/InputSettings.hpp"
#include "desktop/network/NetworkStatus.hpp"
#include "desktop/power/PowerSettings.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include "desktop/system/SystemStatus.hpp"
#include "desktop/system/SystemTools.hpp"
#include "desktop/system/UpdateChecker.hpp"
#include "desktop/theme/ThemeSync.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "desktop/weather/WeatherStatus.hpp"
#include "shell/launcher/OrbitSettings.hpp"
#include "shell/modules/ShellModules.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <glib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <poll.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace LunaDash {
namespace {

using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;
using Templates::WaylandSlot;

std::filesystem::path filePath(const QString &path) {
  return std::filesystem::path(path.toStdString());
}

bool fileExists(const QString &path) {
  std::error_code error;
  const bool exists = std::filesystem::exists(filePath(path), error);
  return exists && !error;
}

bool fileIsExecutable(const QString &path) {
  std::error_code error;
  const auto permissions = std::filesystem::status(filePath(path), error).permissions();
  using Permissions = std::filesystem::perms;
  return !error &&
         (permissions & (Permissions::owner_exec | Permissions::group_exec |
                         Permissions::others_exec)) != Permissions::none;
}

bool createDirectories(const QString &path) {
  std::error_code error;
  std::filesystem::create_directories(filePath(path), error);
  return !error && std::filesystem::is_directory(filePath(path), error) &&
         !error;
}

QString executableDirectory() {
  std::error_code error;
  const auto executable =
      std::filesystem::read_symlink("/proc/self/exe", error);
  return error ? QString{} : QString::fromStdString(
                                  executable.parent_path().string());
}

QString findExecutable(const QString &name) {
  const QByteArray encodedName = name.toUtf8();
  gchar *path = g_find_program_in_path(encodedName.constData());
  const QString result = path ? QString::fromUtf8(path) : QString{};
  g_free(path);
  return result;
}

QString locateDataFile(const QString &relativePath) {
  const auto relative = std::filesystem::path(relativePath.toStdString());
  const auto findIn = [&relative](const char *base) -> QString {
    if (!base)
      return {};
    const auto candidate = std::filesystem::path(base) / relative;
    std::error_code error;
    if (!std::filesystem::is_regular_file(candidate, error) || error)
      return {};
    return QString::fromStdString(candidate.string());
  };
  if (auto path = findIn(g_get_user_data_dir()); !path.isEmpty())
    return path;
  for (const char *const *directory = g_get_system_data_dirs(); *directory;
       ++directory)
    if (auto path = findIn(*directory); !path.isEmpty())
      return path;
  return {};
}

QString localFileUri(const std::filesystem::path &path) {
  GError *error = nullptr;
  gchar *uri = g_filename_to_uri(path.c_str(), nullptr, &error);
  if (!uri) {
    if (error)
      g_error_free(error);
    return {};
  }
  const QString result = QString::fromUtf8(uri);
  g_free(uri);
  return result;
}

nlohmann::json nativeJsonObject(const QJsonObject &object) {
  const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
  return nlohmann::json::parse(bytes.constData(),
                               bytes.constData() + bytes.size(), nullptr,
                               false);
}

QJsonObject qtJsonObject(const nlohmann::json &object) {
  const auto bytes = QByteArray::fromStdString(object.dump());
  return QJsonDocument::fromJson(bytes).object();
}

QJsonObject synchronizeApplicationThemeAtQtBoundary(
    const nlohmann::json &preferences, const nlohmann::json &palette) {
  return qtJsonObject(synchronizeApplicationTheme(preferences, palette));
}

std::int64_t currentMillisecondsSinceEpoch() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string currentTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch())
          .count();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm localTime{};
  localtime_r(&time, &localTime);
  std::ostringstream result;
  result << std::put_time(&localTime, "%Y%m%d-%H%M%S-")
         << std::setw(3) << std::setfill('0') << milliseconds % 1000;
  return result.str();
}

SceneAnimationRectangle animationRectangle(const QRect &rectangle) {
  return {rectangle.x(), rectangle.y(), rectangle.width(), rectangle.height()};
}

SceneAnimationOptions sceneAnimationOptions(const QJsonObject &profile) {
  SceneAnimationOptions options;
  options.duration = profile.value("duration").toInt(options.duration);
  options.enterOffset = profile.value("enterOffset").toInt(options.enterOffset);
  options.focusOpacity =
      profile.value("focusOpacity").toDouble(options.focusOpacity);
  options.exitScale = profile.value("exitScale").toDouble(options.exitScale);
  const auto curve = profile.value("easing").toString();
  if (curve == "linear")
    options.curve = SceneAnimationCurve::Linear;
  else if (curve == "outQuint")
    options.curve = SceneAnimationCurve::OutQuint;
  else if (curve == "inOutCubic")
    options.curve = SceneAnimationCurve::InOutCubic;
  return options;
}

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

void applyXWaylandEnvironment(ProcessEnvironment &environment,
                              const XWaylandSupport *support) {
  ProcessEnvironment displayEnvironment;
  for (const auto *name : {"DISPLAY", "XAUTHORITY"}) {
    if (const auto found = environment.find(name); found != environment.end())
      displayEnvironment.emplace(name, found->second);
  }
  if (support)
    support->applyEnvironment(displayEnvironment);
  else {
    displayEnvironment.erase("DISPLAY");
    displayEnvironment.erase("XAUTHORITY");
  }
  for (const auto *name : {"DISPLAY", "XAUTHORITY"}) {
    environment.erase(name);
    if (const auto found = displayEnvironment.find(name);
        found != displayEnvironment.end())
      environment.emplace(name, found->second);
  }
}

} // namespace

WaylandCompositor::WaylandCompositor(const QByteArray &socket, bool fullscreen,
                                     bool startShell,
                                     const QString &rendererPreference)
    : d(std::make_unique<Impl>(this, socket, fullscreen, rendererPreference)) {
  windowTemplate_ = &windowTemplateForKey("tiling");
  windowLayout_ = createWindowLayout(*windowTemplate_);
  windowSwitcher_ = std::make_unique<WindowSwitcher>(d->eventLoop);
  screenCapture_ = new ScreenCapture(d->eventLoop);
  screenCapture_->setCompletion(
      [this](const std::string &path, const std::string &error) {
        if (!path.empty()) {
          const QString capturePath = QString::fromStdString(path);
          lastCapture_ = capturePath;

          QString helper = findExecutable("lunadash-clipboard-history");
          if (helper.isEmpty()) {
            const QString source = QStringLiteral(LUDASH_SCRIPT_SOURCE_DIR) +
                                   "/lunadash-clipboard-history";
            if (fileIsExecutable(source))
              helper = source;
          }

          if (helper.isEmpty()) {
            qWarning("Screenshot saved, but clipboard helper is unavailable.");
          } else {
            spawn({"publish-file", "image/png", capturePath}, helper, false,
                  [](const ProcessResult &result) {
                    if (!result.success)
                      qWarning("Screenshot saved, but publishing it to the "
                               "clipboard failed.");
                  });
          }
        }
        captureError_ = QString::fromStdString(error);
        if (screenshotCompletion_) {
          auto completion = std::move(screenshotCompletion_);
          screenshotCompletion_ = {};
          const bool saved =
                      error.empty() && QString::fromStdString(path) ==
                                           screenshotDestination_ &&
                      fileExists(screenshotDestination_);
          screenshotDestination_.clear();
          completion(saved);
        }
      });
  brightnessSettings_ = new BrightnessSettings(d->eventLoop);
  ddcBrightnessSettings_ = new DdcBrightnessSettings(d->eventLoop);
  shellModules_ = new ShellModules();
  systemStatus_ = new SystemStatus(d->eventLoop);
  weatherStatus_ = new WeatherStatus(d->eventLoop, [] {
    const auto preferences = desktopPreferences();
    return WeatherPreferences{
        preferences.value("weatherEnabled", false),
        preferences.value("weatherLatitude", 0.0),
        preferences.value("weatherLongitude", 0.0),
        preferences.value("weatherLocation", std::string{})};
  });
  d->scheduleCallback(0, [this] {
    const auto path = wallpaperImagePath();
    if (!path.empty())
      refreshWallpaperPalette(d->eventLoop, path, wallpaperIsVideo(path));
  });
  audioSettings_ = new AudioSettings(d->eventLoop);
  powerSettings_ = new PowerSettings(d->eventLoop);
  sessionActions_ = std::make_unique<SessionActions>();
  shortcutSettings_ = new ShortcutSettings();
  updateChecker_ = new UpdateChecker(d->eventLoop, [] {
    return desktopPreferences().value("updateChannel", std::string("stable"));
  });
  networkStatus_ = new NetworkStatus(d->eventLoop);
  pluginManager_ = std::make_unique<PluginManager>(d->eventLoop);
  pluginManager_->loadEnabled();
  windowLayout_->setPlacementFilter(
      [this](auto workspace, auto area, const auto &placements) {
        return pluginWindowPlacements(*pluginManager_, *windowTemplate_,
                                      workspace, area, placements);
      });

  if (!d->initialize())
    return;
  sessionServices_ = std::make_unique<SessionServices>(d->eventLoop);

  pluginManager_->setChangedCallback([this] {
    if (!d->scheduleCallback(0, [this] { arrange(); }))
      qWarning("Could not schedule a plugin change relayout.");
  });
  windowAnimations_ = std::make_unique<SceneWindowAnimationTemplate>();
  const auto initialAppearance = desktopPreferences();
  windowAnimations_->configure(sceneAnimationOptions(windowAnimationProfile(
      *pluginManager_, qtJsonObject(initialAppearance), *windowTemplate_)));

  controlPath_ = qEnvironmentVariable("LUNADASH_CONTROL");
  if (controlPath_.isEmpty())
    controlPath_ = qEnvironmentVariable("LUDASH_CONTROL");
  if (controlPath_.isEmpty())
    controlPath_ =
        QString::fromUtf8(g_get_user_runtime_dir()) + "/" +
        QString::fromUtf8(socket) + "-control";
  windowSwitcher_->setChannelPath(controlPath_ + "-interaction.json");
  controlServer_ = std::make_unique<ControlServer>(
      d->eventLoop, controlPath_.toStdString(),
      [this](std::string_view request) {
        const QByteArray bytes(request.data(),
                               static_cast<qsizetype>(request.size()));
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(bytes, &parseError);
        const auto response =
            parseError.error == QJsonParseError::NoError && document.isObject()
                ? control(document.object())
                : QJsonObject{{"error", "Invalid request"}};
        return QJsonDocument(response)
            .toJson(QJsonDocument::Compact)
            .toStdString();
      });

  ensureLegacyDesktopSettingsImported();
  const auto clientEnvironment = createClientEnvironment(
      socket.toStdString(), controlPath_.toStdString(),
      executableDirectory().toStdString(),
      loadDesktopPreferences());
  clientEnvironment_ = clientEnvironment;
  publishClientEnvironment(clientEnvironment);

  xwayland_ = new XWaylandSupport;
  if (qEnvironmentVariableIntValue("LUDASH_DISABLE_XWAYLAND") != 1) {
    auto environment = clientEnvironment_;
    environment["XCURSOR_SIZE"] =
        std::to_string(desktopPreferences().value("cursorSize", 24));
    if (!xwayland_->start(d->display, d->compositor, d->seat,
                          environment,
                          [this](wlr_xwayland_surface *surface) {
                            if (d)
                              d->addXWaylandSurface(surface);
                          }))
      qWarning(
          "XWayland/XWM could not be prepared; X11 clients are unavailable.");
  }

  shellModules_->setChangedCallback([this] { arrange(); });

  const auto startSessionClients = [this, startShell] {
    if (!startShell || shuttingDown_ || testStopping_)
      return;

    if (qEnvironmentVariableIntValue("LUNADASH_PUBLISH_ACTIVATION_ENV") == 1) {
      const QString agent = findExecutable("lunadash-polkit-agent");
      if (!agent.isEmpty())
        spawn({}, agent, false);

      if (qEnvironmentVariableIntValue("LUNADASH_DISABLE_FCITX") != 1) {
        const QString fcitx = findExecutable("fcitx5");
        if (!fcitx.isEmpty())
          spawn({"--replace"}, fcitx, false);
      }
    }

    spawn({"--session"}, {}, false);

    if (setupComplete()) {
      d->scheduleCallback(1200, [this] {
        if (testStopping_ || shuttingDown_)
          return;
        for (const auto &app : desktopPreferences().value("startupApps", nlohmann::json::array()))
          if (app.is_string())
            spawn({"--app", QString::fromStdString(app.get<std::string>())});
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
  cancelWallpaperPalette();
  delete shellModules_;
  shellModules_ = nullptr;
  sessionServices_.reset();
  delete screenCapture_;
  screenCapture_ = nullptr;
  delete audioSettings_;
  audioSettings_ = nullptr;
  delete powerSettings_;
  powerSettings_ = nullptr;
  delete updateChecker_;
  updateChecker_ = nullptr;
  delete brightnessSettings_;
  brightnessSettings_ = nullptr;
  delete ddcBrightnessSettings_;
  ddcBrightnessSettings_ = nullptr;
  delete networkStatus_;
  networkStatus_ = nullptr;
  delete systemStatus_;
  systemStatus_ = nullptr;
  delete weatherStatus_;
  weatherStatus_ = nullptr;
  if (windowAnimations_)
    windowAnimations_->clear();
  if (xwayland_) {
    xwayland_->stop();
    delete xwayland_;
    xwayland_ = nullptr;
  }
  const auto childProcesses = processes_;
  processes_.clear();
  for (const auto &process : childProcesses) {
    if (!process)
      continue;
    process->cancel();
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
  qWarning().noquote() << "LunaDash shell restart scheduled in" << delayMs
                       << "ms";

  d->scheduleCallback(delayMs, [this] {
    shellRestartScheduled_ = false;
    if (shuttingDown_ || testStopping_ || logoutPending_)
      return;
    spawn({"--session", "--no-welcome"}, {}, false);
  });
}

ProcessRunner *WaylandCompositor::spawn(
    const QStringList &arguments, const QString &program, bool required,
    std::function<void(const ProcessResult &)> completion) {
  auto environment = clientEnvironment_;
  applyXWaylandEnvironment(environment, xwayland_);

  const bool sessionShell =
      program.isEmpty() &&
      std::find(arguments.cbegin(), arguments.cend(), "--session") !=
          arguments.cend();
  std::vector<std::string> nativeArguments;
  QString executable = program;
  if (sessionShell) {

    const QString sourceConfig =
        QStringLiteral(LUDASH_QML_SOURCE_DIR) + "/shell.qml";
    // Use assets next to the installed executable before any build checkout.
    // This also supports a staged/relocated installation in CI.
    const auto configPath =
        (filePath(executableDirectory()) /
         "../share/lunadash/shell/shell.qml")
            .lexically_normal();
    QString config = QString::fromStdString(configPath.string());
    if (!fileExists(config))
      config = fileExists(sourceConfig)
                   ? sourceConfig
                   : locateDataFile("lunadash/shell/shell.qml");
    if (config.isEmpty())
      config = locateDataFile("ludash/shell/shell.qml");
    if (config.isEmpty()) {
      if (required)
        processFailure_ = true;
      qWarning("LunaDash shell.qml was not found in any search location.");
      return nullptr;
    }
    const QString quickshell = findExecutable("quickshell");
    if (quickshell.isEmpty()) {
      if (required)
        processFailure_ = true;
      qWarning("Quickshell is unavailable.");
      return nullptr;
    }
    const auto wallpaperPath = wallpaperImagePath();
    environment["LUNADASH_WALLPAPER"] =
        wallpaperPath.empty()
            ? std::string{}
            : localFileUri(wallpaperPath).toStdString();
    // Qt Multimedia's FFmpeg backend can use NVDEC through CUDA. Prefer it
    // only for the LunaDash shell on NVIDIA hosts and only when the user did
    // not choose a backend explicitly. Unsupported systems still use Qt's
    // normal software fallback.
    const auto decoderPreference =
        environment.find("QT_FFMPEG_DECODING_HW_DEVICE_TYPES");
    if ((decoderPreference == environment.end() ||
         decoderPreference->second.empty()) &&
        (fileExists("/sys/module/nvidia") ||
         fileExists("/proc/driver/nvidia/version")))
      environment["QT_FFMPEG_DECODING_HW_DEVICE_TYPES"] = "cuda";
    // The shell draws its own theme. A blocking portal Settings query here
    // would prevent even the startup splash from reaching its first frame.
    environment["QT_QPA_PLATFORMTHEME"] = "generic";
    environment.erase("GTK_USE_PORTAL");
    qInfo().noquote() << "LunaDash shell config:" << config;
    ShellRenderingEnvironment shellRenderingEnvironment;
    for (const auto *key :
         {"LUDASH_SHELL_RENDERER", "QT_QUICK_BACKEND", "QSG_RHI_BACKEND"}) {
      const auto value = environment.find(key);
      if (value != environment.end())
        shellRenderingEnvironment.emplace(key, value->second);
    }
    if (!configureShellRendering(shellRenderingEnvironment,
                                 fileExists("/sys/module/nvidia"))) {
      qWarning(
          "LUDASH_SHELL_RENDERER must be auto, opengl, vulkan, or software.");
      processFailure_ = true;
      return nullptr;
    }
    for (const auto *key :
         {"LUDASH_SHELL_RENDERER", "QT_QUICK_BACKEND", "QSG_RHI_BACKEND"}) {
      const auto value = shellRenderingEnvironment.find(key);
      if (value == shellRenderingEnvironment.end())
        environment.erase(key);
      else
        environment[key] = value->second;
    }
    executable = quickshell;
    nativeArguments = {"--path", config.toStdString(), "--no-color"};
  } else {
    if (executable.isEmpty()) {
      if (arguments.size() >= 2 && arguments[0] == "--app" &&
          arguments[1] == "welcome") {
        executable = executableDirectory() + "/lunadash-welcome";
        if (!fileExists(executable))
          executable = executableDirectory() + "/ludash-welcome";
      } else {
        executable = executableDirectory() + "/lunadash-desktop";
        if (!fileExists(executable))
          executable = executableDirectory() + "/ludash-desktop";
      }
    }
    if (!(arguments.size() >= 2 && arguments[0] == "--app" &&
          arguments[1] == "welcome"))
      for (const auto &argument : arguments)
        nativeArguments.push_back(argument.toStdString());
  }

  if (executable.isEmpty()) {
    if (required)
      processFailure_ = true;
    return nullptr;
  }

  auto process = std::make_shared<ProcessRunner>(d->eventLoop, false, true);
  auto *raw = process.get();
  processes_.push_back(process);
  const std::weak_ptr<ProcessRunner> weakProcess = process;
  const bool started = process->runDetailed(
      executable.toStdString(), nativeArguments,
      [this, raw, weakProcess, required, sessionShell,
       completion = std::move(completion)](const ProcessResult &result) {
        if (!shuttingDown_ && !result.success) {
          if (required)
            processFailure_ = true;
          qWarning("LunaDash child process exited unsuccessfully.");
        }
        if (completion)
          completion(result);
        if (sessionShell && !shuttingDown_ && !testStopping_ &&
            !logoutPending_) {
          qWarning("LunaDash shell exited; scheduling restart.");
          scheduleShellRestart();
        }
        const auto weakOwner = weakProcess;
        if (d)
          d->scheduleCallback(0, [this, raw, weakOwner] {
            const auto locked = weakOwner.lock();
            if (locked && !locked->busy())
              std::erase(processes_, locked);
            else if (!locked)
              std::erase_if(processes_, [raw](const auto &entry) {
                return entry.get() == raw;
              });
          });
      },
      0, environment);
  if (!started) {
    std::erase(processes_, process);
    if (required)
      processFailure_ = true;
    return nullptr;
  }
  if (sessionShell)
    d->scheduleCallback(10000, [this, weakProcess] {
      if (const auto active = weakProcess.lock(); active && active->busy())
        shellRestartFailures_ = 0;
    });
  return raw;
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
  if (x11Helper) {
    std::string xwaylandError;
    if (!xwayland_ || !xwayland_->startServer(&xwaylandError)) {
      if (xwaylandError.empty())
        xwaylandError =
            "This application's helper requires XWayland. Install and enable "
            "it, then restart LunaDash.";
      if (error)
        *error = QString::fromStdString(xwaylandError);
      return false;
    }
  }

  const QString executable = command.takeFirst();
  spawn(command, executable, false);
  return true;
}

bool WaylandCompositor::saveScreenshot(
    const QString &path, const std::function<void(bool)> &finished) {
  if (!d || !d->primaryOutput || !filePath(path).is_absolute() ||
      fileExists(path) || screenCapture_->busy())
    return false;

  captureError_.clear();
  screenshotDestination_ = finished ? path : QString{};
  screenshotCompletion_ = finished;

  wlr_output_schedule_frame(d->primaryOutput);
  std::string error;
  if (!screenCapture_->captureOutput(clientEnvironment_, path.toStdString(),
                                     &error)) {
    screenshotDestination_.clear();
    screenshotCompletion_ = {};
    captureError_ = QString::fromStdString(error);
    return false;
  }
  return true;
}

void WaylandCompositor::saveState(const QString &path) {
  std::ofstream file(filePath(path), std::ios::binary | std::ios::trunc);
  if (!file)
    return;
  const auto bytes = QJsonDocument(state()).toJson(QJsonDocument::Indented);
  file.write(bytes.constData(), bytes.size());
}

bool WaylandCompositor::hasProcessFailure() const { return processFailure_; }

int WaylandCompositor::runEventLoop(bool profile) {
  return d ? d->runEventLoop(profile) : 2;
}

void WaylandCompositor::terminateEventLoop(int exitCode) {
  if (d)
    d->terminateEventLoop(exitCode);
}

bool WaylandCompositor::scheduleEventLoopCallback(
    int delayMilliseconds, std::function<void()> callback) {
  return d && d->scheduleCallback(delayMilliseconds, std::move(callback));
}

QJsonObject WaylandCompositor::currentWindowLayoutSettings() const {
  if (!windowTemplate_)
    return {};

  auto values = windowTemplate_->layoutSettingsDefaults;

  // Preserve legacy gap preferences until the user edits the new template API.
  if (windowTemplate_->key == "tiling") {
    const auto preferences = desktopPreferences();
    if (preferences.contains("gap"))
      values["gap"] = preferences.at("gap").get<int>();
    const int legacyGap =
        configuredBuiltinSettings("window-layout").value("gap").toInt(-1);
    if (legacyGap >= 0)
      values["gap"] = legacyGap;
  }

  std::string storageError;
  const auto stored = LunaDash::loadWindowLayoutSettings(
      windowTemplate_->key.toStdString(),
      nativeJsonObject(windowTemplate_->layoutSettingsSchema), &storageError);
  if (!storageError.empty())
    qWarning("Could not load window layout settings: %s",
             storageError.c_str());
  const auto storedValues = qtJsonObject(stored);
  for (auto it = storedValues.begin(); it != storedValues.end(); ++it)
    values[it.key()] = it.value();

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

  std::string storageError;
  if (!LunaDash::saveWindowLayoutSettings(
          windowTemplate_->key.toStdString(), nativeJsonObject(values),
          &storageError)) {
    if (error)
      *error = QString::fromStdString(storageError);
    return false;
  }
  return true;
}

QRect WaylandCompositor::workArea() const {
  QRect area = d ? d->usableArea : QRect(0, 0, 1440, 900);
  const auto settings = currentWindowLayoutSettings();
  const int gap = settings.contains("gap")
                      ? settings.value("gap").toInt()
                      : desktopPreferences().value("gap", 12);
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
    client->appId =
        safeUtf8(client->xwayland->class_ ? client->xwayland->class_
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
        client->sceneTree, animationRectangle(client->geometry),
        animationRectangle(rectangle), d->animationLayer,
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
    const auto x =
        static_cast<int16_t>(std::clamp(rectangle.x(), -32768, 32767));
    const auto y =
        static_cast<int16_t>(std::clamp(rectangle.y(), -32768, 32767));
    const auto width =
        static_cast<uint16_t>(std::clamp(size.width(), 1, 65535));
    const auto height =
        static_cast<uint16_t>(std::clamp(size.height(), 1, 65535));
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
  d->eyeCareActive = preferences.value("eyeCare", false);
  d->steadyWindowOpacity =
      d->eyeCareActive ? 1.0f
                       : preferences.value("windowOpacity", 90) / 100.0f;
  if (d->windowGlass) {
    const bool changed = d->windowGlass->configure(
        preferences.value("blur", true) && !d->eyeCareActive,
        preferences.value("blurRadius", 18), d->steadyWindowOpacity);
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
    windowAnimations_->configure(sceneAnimationOptions(windowAnimationProfile(
        *pluginManager_, qtJsonObject(preferences), *windowTemplate_)));
  const int count = preferences.value("workspaceCount", 10);
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
      if (!client->preferredFloatingSize.isValid() && initialWidth > 0 &&
          initialHeight > 0)
        client->preferredFloatingSize = QSize(initialWidth, initialHeight);
    }
    if (managed) {
      const QSize preferred = windowTemplate_ && windowTemplate_->allowOverlap
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
  std::vector<ClientWindow *> revealed;
  std::unordered_map<int, LayoutWindowId> maximizedWindows;
  for (const auto &client : clients_) {
    auto maximizedIt = maximizedWindows.find(client->workspace);
    if (maximizedIt == maximizedWindows.end())
      maximizedIt = maximizedWindows
                        .emplace(client->workspace,
                                 windowLayout_->snapshot(client->workspace)
                                     .maximizedWindow)
                        .first;
    const auto maximized = maximizedIt->second;
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
    client->hiddenByMaximize = windowHiddenByMaximize(
        *windowTemplate_, maximized, inMaximizedFamily, *client);

    bool inFullscreenFamily =
        fullscreenClient && client.get() == fullscreenClient;
    auto *fullscreenParent =
        client->toplevel ? client->toplevel->parent : nullptr;
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
          unmanagedX11 && d->overlayLayer ? d->overlayLayer
          : fullscreenClient && inFullscreenFamily && d->fullscreenLayer
              ? d->fullscreenLayer
              : d->normalLayer;
      if (client->sceneTree->node.parent != targetLayer)
        wlr_scene_node_reparent(&client->sceneTree->node, targetLayer);
      const bool wasVisible = client->sceneTree->node.enabled;
      if (!visible && wasVisible && client->mapped && windowAnimations_)
        windowAnimations_->close(client->sceneTree, d->animationLayer);
      if (visible && !wasVisible && client->geometry.isValid())
        revealed.push_back(client.get());
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
                                  ? client->manualGeometry
                                  : defaultGeometry;
      floatingGeometry.setSize(floatingGeometry.size().boundedTo(area.size()));
      floatingGeometry.moveLeft(
          std::clamp(floatingGeometry.x(), area.left(),
                     area.right() - floatingGeometry.width() + 1));
      floatingGeometry.moveTop(
          std::clamp(floatingGeometry.y(), area.top(),
                     area.bottom() - floatingGeometry.height() + 1));
      configure(client.get(), client->maximized ? area : floatingGeometry);
    }
  }

  // Pending clients need their first size even when a rule sends them to a
  // different workspace, or an existing fullscreen window covers this one.
  std::vector<int> layoutWorkspaces{workspace_};
  for (const auto &client : clients_)
    if (client->layoutPending &&
        std::find(layoutWorkspaces.begin(), layoutWorkspaces.end(),
                  client->workspace) == layoutWorkspaces.end())
      layoutWorkspaces.push_back(client->workspace);
  std::vector<WindowPlacement> placements;
  for (const int workspace : layoutWorkspaces)
    for (auto &placement : windowLayout_->presentation(workspace, area))
      placements.push_back(std::move(placement));
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
      windowAnimations_->open(client->sceneTree,
                              animationRectangle(client->geometry));

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

  if (!client->fullscreen && client->floating && client->hiddenByMaximize) {
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
    windowAnimations_->focus(client->sceneTree,
                             animationRectangle(client->geometry));
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
  std::vector<ClientWindow *> visible;
  for (const auto &client : clients_)
    if (client->mapped && !client->minimized && !client->utility &&
        client->workspace == workspace_)
      visible.push_back(client.get());
  if (visible.empty()) {
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
  const auto found = std::find(visible.begin(), visible.end(), focused_);
  const auto index =
      found == visible.end() ? std::ptrdiff_t{-1}
                             : std::distance(visible.begin(), found);
  const auto count = static_cast<std::ptrdiff_t>(visible.size());
  focus(visible[static_cast<std::size_t>((index + direction + count) % count)]);
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
            if (other->toplevel && other->surface &&
                other->surface->initialized)
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
    terminateEventLoop(hasProcessFailure() ? 2 : 0);
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

void WaylandCompositor::handleShortcut(std::string_view action) {
  const auto workspaceNumber = [action](std::string_view prefix) {
    int target = 0;
    const auto suffix = action.substr(prefix.size());
    const auto parsed =
        std::from_chars(suffix.data(), suffix.data() + suffix.size(), target);
    return action.starts_with(prefix) &&
                   parsed.ec == std::errc{} &&
                   parsed.ptr == suffix.data() + suffix.size()
               ? target
               : 0;
  };
  if (action.starts_with("workspace")) {
    const int target = workspaceNumber("workspace");
    if (target > 0 &&
        target <= desktopPreferences().value("workspaceCount", 10)) {
      selectWorkspace(target - 1);
    }
    return;
  }

  if (action.starts_with("moveToWorkspace")) {
    const int target = workspaceNumber("moveToWorkspace");
    if (target > 0 && focused_ &&
        target <= desktopPreferences().value("workspaceCount", 10)) {
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

  const auto performLayoutAction = [this](const QString &id,
                                          const QJsonObject &payload) {
    return windowTemplate_ &&
           performWindowLayoutAction(*windowLayout_, *windowTemplate_, id,
                                     payload);
  };

  if (action == "focusLeft")
    performLayoutAction("focus-direction",
                        {{"workspace", workspace_}, {"dx", -1}, {"dy", 0}});
  else if (action == "focusRight")
    performLayoutAction("focus-direction",
                        {{"workspace", workspace_}, {"dx", 1}, {"dy", 0}});
  else if (action == "focusUp")
    performLayoutAction("focus-direction",
                        {{"workspace", workspace_}, {"dx", 0}, {"dy", -1}});
  else if (action == "focusDown")
    performLayoutAction("focus-direction",
                        {{"workspace", workspace_}, {"dx", 0}, {"dy", 1}});
  else if (action == "reorderLeft" && focused_)
    performLayoutAction("reorder",
                        {{"window", focused_->id}, {"direction", -1}});
  else if (action == "reorderRight" && focused_)
    performLayoutAction("reorder",
                        {{"window", focused_->id}, {"direction", 1}});
  else if ((action == "groupLeft" || action == "groupRight") && focused_)
    performLayoutAction("group-direction",
                        {{"window", focused_->id},
                         {"workspace", workspace_},
                         {"dx", action == "groupLeft" ? -1 : 1},
                         {"dy", 0}});
  else if (action == "expelWindow" && focused_)
    performLayoutAction("expel", {{"window", focused_->id}});
  else if (action == "centerColumn" && focused_) {
    const auto area = workArea();
    performLayoutAction("center",
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
  } else if (action == "closeWindow" && focused_)
    closeClient(focused_);
  else if (action == "minimizeWindow" && focused_) {
    focused_->minimized = true;
    windowLayout_->setMinimized(focused_->id, true);
  } else if (action == "launchTerminal")
    control({{"method", "launch-default"}, {"value", "terminal"}});
  else if (action == "launchFiles")
    control({{"method", "launch-default"}, {"value", "files"}});
  else if (action == "toggleScratchpad") {
    QString error;
    toggleScratchpad(&error);
    if (!error.isEmpty())
      scratchpadError_ = error;
    return;
  } else if (action == "toggleEyeCare") {
    control({{"method", "eye-care"}, {"value", "toggle"}});
  } else if (action == "chooseWallpaper") {
    control({{"method", "choose-wallpaper"}});
  } else if (action == "randomWallpaper") {
    control({{"method", "wallpaper-random"}});
  } else if (action == "launchOrbit" || action == "openControlCenter" ||
             action == "openClipboard" || action == "openPowerMenu") {
    shellAction_ = QString::fromStdString(std::string(action));
    shellActionSerial_ =
        shellActionSerial_ >= 999999 ? 1 : shellActionSerial_ + 1;
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

QJsonObject WaylandCompositor::state(bool includeTranslations) const {
  QJsonArray entries;
  QJsonArray xwaylandUtilities;
  for (const auto &client : clients_) {
    if (client->utility) {
      // Keep notifications out of the task list while exposing their lifetime
      // separately for diagnostics, including the interval before destruction.
      if (client->x11)
        xwaylandUtilities.append(
            QJsonObject{{"id", client->id}, {"mapped", client->mapped}});
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
    const auto fallback = animationRectangle(client->geometry);
    const auto frame = windowAnimations_
                           ? windowAnimations_->animation().visualGeometry(
                                 client->sceneTree, fallback)
                           : fallback;
    entries.append(QJsonObject{
        {"contentX", contentGeometry.x},
        {"contentY", contentGeometry.y},
        {"contentGeometryWidth",
         client->surface ? contentGeometry.width : bufferWidth},
        {"contentGeometryHeight",
         client->surface ? contentGeometry.height : bufferHeight},
        {"frameX", frame.left},
        {"frameY", frame.top},
        {"frameWidth", frame.width},
        {"frameHeight", frame.height},
        {"cornerRadius", d && d->windowCorners
                             ? d->windowCorners->radius(client->sceneTree)
                             : 0},
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
  std::unordered_set<int> emittedGroups;
  int fallbackGroup = 0;
  for (const auto &column : tilingSnapshot.columns) {
    const int group = column.metadata.value("group").toInt(fallbackGroup++);
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
  preferences["plugins"] =
      nativeJsonObject(extensions).value("installed", nlohmann::json::array());
  const auto layoutSettings = currentWindowLayoutSettings();
  const auto layoutTarget = Settings::target(
      "layout:" + windowTemplate_->key, "Window layout", "layout", "Windows",
      windowTemplate_->layoutSettingsSchema, layoutSettings);
  const auto moduleState = qtJsonObject(shellModules_->snapshot());
  const auto nativeXWayland =
      xwayland_ ? xwayland_->snapshot() : XWaylandSnapshot{};
  QJsonObject xwaylandState{
      {"available", nativeXWayland.available},
      {"mode", "rootless-lazy-xwm"},
      {"running", nativeXWayland.running},
      {"rootWindowVisible", false},
      {"selectionBridge", nativeXWayland.selectionBridge},
      {"display", QString::fromStdString(nativeXWayland.display)},
      {"authority", QString()},
      {"error", QString::fromStdString(nativeXWayland.error)}};
  xwaylandState["utilitySurfaces"] = xwaylandUtilities;
  const auto language = selectedLanguage();
  const auto translations =
      includeTranslations ? qtJsonObject(languageDictionary(language))
                         : QJsonObject{};
  auto *physicalKeyboard = d ? d->preferredKeyboard() : nullptr;
  const auto lockEnabled = [physicalKeyboard](const char *name) {
    if (!physicalKeyboard || !physicalKeyboard->keymap)
      return false;
    const auto index = xkb_keymap_mod_get_index(physicalKeyboard->keymap, name);
    return index != XKB_MOD_INVALID &&
           (physicalKeyboard->modifiers.locked & (xkb_mod_mask_t{1} << index));
  };
  QJsonObject result{
      {"version", QString::fromLatin1(BuildConfig::version)},
      {"settingsApi",
       QJsonObject{{"version", 1},
                   {"targets", settingsApiTargets(extensions, moduleState,
                                                  layoutTarget)}}},
      {"layoutMode", windowTemplate_ && windowTemplate_->allowOverlap
                         ? "stacking"
                         : "tiling"},
      {"windowLayout",
       QJsonObject{
           {"template", windowTemplate_ ? windowTemplate_->key : QString()},
           {"allowOverlap",
            windowTemplate_ ? windowTemplate_->allowOverlap : false},
           {"schema", windowTemplate_ ? windowTemplate_->layoutSettingsSchema
                                      : QJsonObject{}},
           {"values", layoutSettings},
           {"actions",
            windowTemplate_ ? windowTemplate_->layoutActions : QJsonObject{}}}},
      {"defaultApps", QJsonDocument::fromJson(QByteArray::fromStdString(
                                                  defaultApplications().dump()))
                          .object()},
      {"shellModules", moduleState},
      {"extensions", extensions},
      {"shellAnimationDuration",
       shellAnimationDuration(*pluginManager_,
                              preferences.value("animationDuration", 220))},
      {"panelExtent",
       shellModules_->panelExtent(preferences.value("panelHeight", 40))},
      {"workArea", QJsonObject{{"x", workArea().x()},
                               {"y", workArea().y()},
                               {"width", workArea().width()},
                               {"height", workArea().height()}}},
      {"panelEdge", QString::fromStdString(shellModules_->panelEdge())},
      {"panelAtBottom", shellModules_->panelAtBottom()},
      {"audio", QJsonDocument::fromJson(QByteArray::fromStdString(
                                            audioSettings_->snapshot().dump()))
                    .object()},
      {"power", QJsonDocument::fromJson(QByteArray::fromStdString(
                                            powerSettings_->snapshot().dump()))
                    .object()},
      {"sessionActions",
       [this] {
         const auto snapshot = sessionActions_->snapshot();
         QJsonObject result;
         for (const auto &[action, available] : snapshot.availability)
           result.insert(QString::fromStdString(action), available);
         result.insert("error", QString::fromStdString(snapshot.error));
         return result;
       }()},
      {"shortcuts",
       QJsonDocument::fromJson(
           QByteArray::fromStdString(shortcutSettings_->snapshot().dump()))
           .object()},
      {"update", QJsonDocument::fromJson(QByteArray::fromStdString(
                                             updateChecker_->snapshot().dump()))
                     .object()},
      {"settingsTools",
       [] {
         QJsonArray result;
         for (const auto &tool : systemSettingsTools())
           result.append(
               QJsonObject{{"id", QString::fromStdString(tool.id)},
                           {"category", QString::fromStdString(tool.category)},
                           {"name", QString::fromStdString(tool.name)},
                           {"package", QString::fromStdString(tool.package)},
                           {"available", tool.available},
                           {"configurable", tool.configurable},
                           {"host", tool.host}});
         return result;
       }()},
      {"display", d ? d->displaySnapshot() : QJsonObject{}},
      {"settingsSerial", settingsSerial_},
      {"settingsPage", settingsPage_},
      {"pickerSerial", pickerSerial_},
      {"launcherSerial", launcherSerial_},
      {"launcherOpen", launcherVisible_},
      {"input",
       QJsonObject{{"layout", QString::fromStdString(keyboardLayoutPreference(
                                  preferences.value("keyboardLayout", std::string("us"))))},
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
      {"blurFrames",
       static_cast<qint64>(d && d->windowGlass ? d->windowGlass->frames() : 0)},
      {"blurError", d && d->windowGlass
                        ? QString::fromStdString(d->windowGlass->error())
                        : QString()},
      {"activeAnimations",
       windowAnimations_ ? windowAnimations_->activeCount() : 0},
      {"xwayland", xwaylandState},
      {"screenCapture", QJsonObject{{"protocol", "zwlr_screencopy_manager_v1"},
                                    {"version", 3},
                                    {"frames", 0},
                                    {"lastCapture", lastCapture_},
                                    {"error", captureError_},
                                    {"busy", screenCapture_->busy()},
                                    {"phase", QString::fromStdString(
                                                  screenCapture_->phase())}}},
      {"activationEnvironment",
       QJsonObject{{"published", activationEnvironmentPublished_},
                   {"error", activationEnvironmentError_}}},
      {"brightness",
       QJsonDocument::fromJson(
           QByteArray::fromStdString(brightnessSettings_->snapshot().dump()))
           .object()},
      {"ddcBrightness",
       QJsonDocument::fromJson(
           QByteArray::fromStdString(ddcBrightnessSettings_->snapshot().dump()))
           .object()},
      {"appearance", qtJsonObject(preferences)},
      {"palette", qtJsonObject(palette)},
      {"applicationTheme",
       synchronizeApplicationThemeAtQtBoundary(preferences, palette)},
      {"appearancePresets",
       QJsonDocument::fromJson(
           QByteArray::fromStdString(appearancePresets().dump()))
           .array()},
      {"orbit", qtJsonObject(orbitSettings())},
      {"wallpapers",
       QJsonDocument::fromJson(
           QByteArray::fromStdString(wallpaperSnapshot().dump()))
           .object()},
      {"shellAction", shellAction_},
      {"shellActionSerial", shellActionSerial_},
      {"scratchpad", QJsonObject{{"window", scratchpadWindow_},
                                 {"pending", scratchpadPending_},
                                 {"error", scratchpadError_}}},
      {"setupComplete", setupComplete()},
      {"network",
       QJsonDocument::fromJson(
           QByteArray::fromStdString(networkStatus_->snapshot().dump()))
           .object()},
      {"system", QJsonDocument::fromJson(QByteArray::fromStdString(
                                             systemStatus_->snapshot().dump()))
                     .object()},
      {"weather",
       QJsonDocument::fromJson(
           QByteArray::fromStdString(weatherStatus_->snapshot().dump()))
           .object()},
      {"wallpaperImage",
       wallpaperImagePath().empty()
           ? QString{}
           : localFileUri(wallpaperImagePath())},
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
      {"xdgPopupCount",
       d ? static_cast<qint64>(d->xdgPopups.size()) : qint64{0}},
      {"language", QString::fromStdString(language)},
      {"translations", translations},
      {"wallpaper", loadWallpaperSettings().value("shader", 0)},
      {"shutdown", testStopping_},
      {"graphicsApi", "wlroots"},
      {"renderer",
       QString::fromLatin1(ludash_renderer_name(d ? d->renderer : nullptr))},
      {"vulkanAvailable", ludash_renderer_vulkan_available()},
      {"shaderReady", d && d->renderer},
      {"graphicsFailed", processFailure_},
      {"graphicsMajor", 0},
      {"graphicsMinor", 0}};
  if (!includeTranslations)
    result.remove("translations");
  return result;
}

QJsonObject WaylandCompositor::control(const QJsonObject &request) {
  const QString method = request.value("method").toString();
  const QString value = request.value("value").toString();
  if (method == "status")
    return state(value.toStdString() != selectedLanguage());

  bool numberValid = false;
  const int number = value.toInt(&numberValid);

  if (method == "workspace" && numberValid && number >= 0 &&
      number < desktopPreferences().value("workspaceCount", 10)) {
    selectWorkspace(number);
  } else if (method == "language" &&
             isSupportedLanguage(value.toStdString())) {
    std::string error;
    if (!setSelectedLanguage(value.toStdString(), &error))
      return {{"error", QString::fromStdString(error)}};
  } else if (method == "shortcut-capture" &&
             (value == "true" || value == "false")) {
    shortcutCapture_ = value == "true";
  } else if (method == "shortcuts") {
    const auto changes =
        nlohmann::json::parse(value.toStdString(), nullptr, false);
    std::string error;
    if (changes.is_discarded() || !shortcutSettings_->apply(changes, &error))
      return {{"error", error.empty() ? "Expected shortcut JSON."
                                      : QString::fromStdString(error)}};
  } else if (method == "reset-shortcuts") {
    std::string error;
    if (!shortcutSettings_->reset(&error))
      return {{"error", QString::fromStdString(error)}};
  } else if (method == "capture") {
    if (!value.startsWith('/'))
      return {{"error", "Capture requires an absolute path."}};
    if (fileExists(value))
      return {{"error", "Refusing to overwrite " + value}};
    if (!saveScreenshot(value))
      return {{"error", captureError_.isEmpty()
                            ? "Could not start capture for " + value
                            : captureError_}};
    return {{"path", value},
            {"pending", true},
            {"phase", QString::fromStdString(screenCapture_->phase())}};
  } else if (method == "screenshot") {
    captureScreen();
    if (!captureError_.isEmpty())
      return {{"error", captureError_}};
    return {{"pending", screenCapture_->busy()},
            {"phase", QString::fromStdString(screenCapture_->phase())}};
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
        static_cast<uint32_t>(currentMillisecondsSinceEpoch() & 0xffffffff);
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
    const auto layout = Settings::target(
        "layout:" + windowTemplate_->key, "Window layout", "layout", "Windows",
        windowTemplate_->layoutSettingsSchema, currentWindowLayoutSettings());
    QString error;
    if (!updateSettingsApi(
            *pluginManager_, *shellModules_, layout, document.object(),
            [this](const auto &changes, QString *message) {
              return updateWindowLayoutSettings(changes, message);
            },
            &error))
      return {{"error", error},
              {"settingsTarget", document.object().value("target")}};
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
    std::string nativeError;
    bool ok = false;
    if (method == "module-validate")
      ok = shellModules_->validate(value.toStdString(), &nativeError);
    else if (method == "module-save")
      ok = shellModules_->apply(value.toStdString(), &nativeError);
    else if (method == "module-reset")
      ok = shellModules_->reset(&nativeError);
    if (!ok)
      return {{"error", nativeError.empty()
                             ? QStringLiteral("Invalid module command.")
                             : QString::fromStdString(nativeError)}};
  } else if (method == "module-error") {
    const auto object = QJsonDocument::fromJson(value.toUtf8()).object();
    shellModules_->reportError(object.value("id").toString().toStdString(),
                               object.value("error").toString().toStdString());
  } else if (method == "default-apps") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    QString error;
    std::string nativeError;
    const auto changes =
        document.isObject()
            ? nlohmann::json::parse(
                  document.toJson(QJsonDocument::Compact).toStdString())
            : nlohmann::json{};
    if (!document.isObject() ||
        !setDefaultApplications(changes, &nativeError)) {
      error = QString::fromStdString(nativeError);
      return {{"error", error.isEmpty() ? "Expected a JSON object." : error}};
    }
  } else if (method == "open-url") {
    QString error;
    const QUrl url(value);
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http"))
      return {{"error", "Only valid HTTP and HTTPS URLs can be opened."}};
    std::string nativeError;
    auto command = defaultApplicationCommand("browser", &nativeError);
    if (!nativeError.empty())
      return {{"error", QString::fromStdString(nativeError)}};
    if (command.empty())
      return {{"error", "No browser is configured."}};
    command.push_back(url.toString(QUrl::FullyEncoded).toStdString());
    QStringList qtCommand;
    for (const auto &argument : command)
      qtCommand.push_back(QString::fromStdString(argument));
    if (!launchExternalCommand(qtCommand, &error))
      return {{"error", error}};
  } else if (method == "launch-default" &&
             (value == "terminal" || value == "files" || value == "browser")) {
    std::string nativeError;
    auto command = defaultApplicationCommand(value.toStdString(), &nativeError);
    QString error = QString::fromStdString(nativeError);
    if (!error.isEmpty())
      return {{"error", error}};
    if (command.empty())
      return {{"error", "No application is configured for this role."}};
    QStringList qtCommand;
    for (const auto &argument : command)
      qtCommand.push_back(QString::fromStdString(argument));
    if (!launchExternalCommand(qtCommand, &error))
      return {{"error", error}};
  } else if (method == "system-tool") {
    auto command = systemSettingsCommand(value.toStdString());
    if (command.empty())
      return {{"error", "This settings tool is unavailable."}};
    const QString program = QString::fromStdString(command.front());
    QStringList arguments;
    for (std::size_t index = 1; index < command.size(); ++index)
      arguments.push_back(QString::fromStdString(command[index]));
    spawn(arguments, program, false);
  } else if (method == "audio") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    std::string nativeError;
    const auto request =
        document.isObject()
            ? nlohmann::json::parse(
                  document.toJson(QJsonDocument::Compact).toStdString())
            : nlohmann::json{};
    if (!document.isObject() || !audioSettings_->apply(request, &nativeError)) {
      const QString error = QString::fromStdString(nativeError);
      return {{"error", error.isEmpty() ? "Invalid audio setting." : error}};
    }
  } else if (method == "network") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    std::string nativeError;
    const auto request =
        document.isObject()
            ? nlohmann::json::parse(
                  document.toJson(QJsonDocument::Compact).toStdString())
            : nlohmann::json{};
    if (!document.isObject() ||
        !networkStatus_->execute(request, &nativeError)) {
      const QString error = QString::fromStdString(nativeError);
      return {{"error", error.isEmpty() ? "Invalid network request." : error}};
    }
  } else if (method == "power-profile") {
    std::string nativeError;
    if (!powerSettings_->apply(value.toStdString(), &nativeError))
      return {{"error", QString::fromStdString(nativeError)}};
  } else if (method == "session-action") {
    std::string error;
    const auto action = value.toStdString();
    if (!isValidSessionAction(action) ||
        !sessionActions_->execute(action, &error))
      return {{"error", error.empty() ? "Invalid session action."
                                      : QString::fromStdString(error)}};
  } else if (method == "brightness") {
    bool ok = false;
    const int percent = value.toInt(&ok);
    std::string nativeError;
    if (!ok || !brightnessSettings_->setPercent(percent, &nativeError)) {
      const QString error = QString::fromStdString(nativeError);
      return {{"error",
               error.isEmpty() ? "Brightness must be an integer." : error}};
    }
  } else if (method == "ddc-brightness") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    const auto request = document.object();
    const auto percent = request.value("percent");
    std::string nativeError;
    if (!document.isObject() || !percent.isDouble() ||
        percent.toDouble() != percent.toInt(-1) ||
        !ddcBrightnessSettings_->setPercent(
            request.value("id").toString().toStdString(), percent.toInt(-1),
            &nativeError)) {
      const QString error = QString::fromStdString(nativeError);
      return {{"error",
               error.isEmpty() ? "Invalid DDC/CI brightness request." : error}};
    }
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
    ensureLegacyDesktopSettingsImported();
    std::string nativeError;
    if (!resetDesktopPreferences(&nativeError))
      return {{"error", QString::fromStdString(nativeError)}};
    weatherStatus_->refresh();
    if (d)
      d->updateNightLight();
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
    const auto action =
        windowTemplate_->layoutActions.value(actionId).toObject();
    if (action.value("requiresArea").toBool()) {
      if (payload.contains("area"))
        return {{"error",
                 "Work area is supplied by the compositor, not the caller."}};
      const auto area = workArea();
      payload["area"] = QJsonObject{{"x", area.x()},
                                    {"y", area.y()},
                                    {"width", area.width()},
                                    {"height", area.height()}};
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
    std::string nativeError;
    const bool saved = method == "orbit-save"
                           ? saveOrbitSettings(value.toStdString(), &nativeError)
                           : resetOrbitSettings(&nativeError);
    if (!saved)
      return {{"error", QString::fromStdString(nativeError)}};
  } else if (method == "appearance-preset-save" ||
             method == "appearance-preset-apply" ||
             method == "appearance-preset-delete") {
    ensureLegacyDesktopSettingsImported();
    std::string nativeError;
    const auto name = value.toStdString();
    const bool ok = method == "appearance-preset-save"
                        ? saveAppearancePreset(name, &nativeError)
                    : method == "appearance-preset-apply"
                        ? applyAppearancePreset(name, &nativeError)
                        : deleteAppearancePreset(name, &nativeError);
    if (!ok)
      return {{"error", QString::fromStdString(nativeError)}};
    if (method == "appearance-preset-apply") {
      const auto updatedPreferences = desktopPreferences();
      synchronizeApplicationThemeAtQtBoundary(
          updatedPreferences, appearancePalette(updatedPreferences));
    }
    if (d)
      d->updateNightLight();
    arrange();
  } else if (method == "scratchpad") {
    QString error;
    if (!toggleScratchpad(&error))
      return {{"error", error}};
  } else if (method == "eye-care") {
    if (value != "toggle" && value != "true" && value != "false")
      return {{"error", "Eye care expects toggle, true or false."}};
    std::string error;
    const bool enabled = value == "toggle"
                             ? !desktopPreferences().value("eyeCare", false)
                             : value == "true";
    if (!updateDesktopPreferences({{"eyeCare", enabled}}, &error))
      return {{"error", QString::fromStdString(error)}};
    if (d)
      d->updateNightLight();
    arrange();
  } else if (method == "wallpaper-random") {
    std::string error;
    if (!selectRandomWallpaper(&error))
      return {{"error", QString::fromStdString(error)}};
    const auto path = wallpaperImagePath();
    refreshWallpaperPalette(d->eventLoop, path, wallpaperIsVideo(path));
  } else if (method == "appearance") {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
    std::string error;
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
      return {{"error", "Expected a JSON object of desktop preferences."}};
    const auto changes = nativeJsonObject(document.object());
    if (!updateDesktopPreferences(changes, &error))
      return {{"error", QString::fromStdString(error)}};
    // Publish color-scheme immediately so portal-aware Chromium/Electron/Qt
    // clients and gsettings-aware GTK applications follow the desktop switch
    // without waiting for the next shell status poll.
    const auto updatedPreferences = desktopPreferences();
    synchronizeApplicationThemeAtQtBoundary(
        updatedPreferences, appearancePalette(updatedPreferences));
    applyKeyboardConfiguration();
    for (const auto &key : {"weatherEnabled", "weatherLatitude",
                            "weatherLongitude", "weatherLocation"})
      if (changes.contains(key)) {
        weatherStatus_->refresh();
        break;
      }
    if (changes.value("wallpaperColors", false)) {
      const auto path = wallpaperImagePath();
      refreshWallpaperPalette(d->eventLoop, path, wallpaperIsVideo(path));
    }
    if (d) {
      d->updateBackground();
      d->updateNightLight();
    }
    arrange();
  } else if (method == "launch-x11") {
    std::vector<std::string> command;
    const auto utf8 = value.toUtf8();
    gchar **parsed = nullptr;
    gint count = 0;
    GError *parseError = nullptr;
    if (!g_shell_parse_argv(utf8.constData(), &count, &parsed, &parseError)) {
      const QString message = parseError
                                  ? QString::fromUtf8(parseError->message)
                                  : QStringLiteral("Invalid command line.");
      if (parseError)
        g_error_free(parseError);
      return {{"error", message}};
    }
    command.reserve(static_cast<std::size_t>(count));
    for (gint index = 0; index < count; ++index)
      command.emplace_back(parsed[index]);
    g_strfreev(parsed);
    std::string error;
    if (!xwayland_ || !xwayland_->launch(command, &error))
      return {{"error", error.empty() ? "XWayland is unavailable."
                                      : QString::fromStdString(error)}};
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
    std::vector<std::string> policyCommand;
    policyCommand.reserve(static_cast<std::size_t>(command.size()));
    for (const auto &argument : command)
      policyCommand.push_back(argument.toStdString());
    const auto capabilities =
        resolveLaunchCapabilities(desktopId.toStdString(), policyCommand);
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
    std::string error;
    if (!setSetupComplete(true, &error))
      return {{"error", QString::fromStdString(error)}};
  } else if (method == "setup") {
    std::string error;
    if (!setSetupComplete(false, &error))
      return {{"error", QString::fromStdString(error)}};
  } else if (method == "configure-network") {
    auto program = findExecutable("nm-connection-editor");
    QStringList arguments;
    if (program.isEmpty() &&
        !findExecutable("nmtui").isEmpty()) {
      for (const auto &terminal : {"kitty", "konsole", "alacritty", "foot"}) {
        program = findExecutable(terminal);
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
    std::string refreshError;
    if (!refreshWallpaperLibrary(&refreshError))
      return {{"error", QString::fromStdString(refreshError)}};
    pickerSerial_ = (pickerSerial_ + 1) % 1000000;
  } else if (method == "wallpaper-image") {
    std::string error;
    const QUrl url(value);
    const auto path = url.isLocalFile()
                          ? std::filesystem::path(url.toLocalFile().toStdString())
                          : std::filesystem::path(value.toStdString());
    if (!setWallpaperImage(path, &error))
      return {{"error", QString::fromStdString(error)}};
    const auto selected = wallpaperImagePath();
    refreshWallpaperPalette(d->eventLoop, selected,
                            wallpaperIsVideo(selected));
  } else if (method == "wallpaper-default") {
    std::string error;
    if (!resetWallpaperImage(&error))
      return {{"error", QString::fromStdString(error)}};
    const auto selected = wallpaperImagePath();
    refreshWallpaperPalette(d->eventLoop, selected,
                            wallpaperIsVideo(selected));
  } else if (method == "wallpaper" && numberValid && number >= 0 &&
             number <= 1) {
    ensureLegacyDesktopSettingsImported();
    std::string nativeError;
    if (!saveWallpaperSettings({{"shader", number}, {"mode", "shader"}},
                               &nativeError))
      return {{"error", QString::fromStdString(nativeError)}};
    if (d)
      d->updateBackground();
  } else if (method == "group-window") {
    int window = 0;
    int target = 0;
    if (!groupWindowIds(value, &window, &target))
      return {{"error", "Expected bounded JSON window and target IDs."}};
    if (!windowTemplate_ || !performWindowLayoutAction(
                                *windowLayout_, *windowTemplate_, "group-with",
                                {{"window", window}, {"target", target}}))
      return {{"error", "Windows cannot be grouped."}};
    arrange();
  } else if (method == "expel-window") {
    const auto window = textWindowId(value);
    if (!window || !windowTemplate_ ||
        !performWindowLayoutAction(*windowLayout_, *windowTemplate_, "expel",
                                   {{"window", *window}}))
      return {
          {"error", "Window layout does not support expelling this window."}};
    arrange();
    synchronizeWindowFocus();
  } else if (method == "quit") {
    if (value != "confirm")
      return {{"error", "Logout requires explicit confirmation."}};
    qInfo("LunaDash logout requested through the control channel.");
    d->scheduleCallback(0, [this] { requestShutdown(); });
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
  applyXWaylandEnvironment(environment, xwayland_);
  if (!sessionServices_) {
    if (ready)
      ready();
    return;
  }
  sessionServices_->publishActivationEnvironment(
      environment,
      [this, ready](bool ok, const std::string &error) {
        activationEnvironmentPublished_ = ok;
        activationEnvironmentError_ = QString::fromStdString(error);
        if (!ok) {
          if (ready)
            ready();
          return;
        }
        sessionServices_->refreshScreencastPortalServices(
            clientEnvironment_, ready);
      });
}

QString WaylandCompositor::nextCapturePath() const {
  const char *pictures = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
  auto directory =
      pictures ? QString::fromUtf8(pictures)
               : QString::fromUtf8(g_get_home_dir()) + "/Pictures";
  directory += "/Screenshots";
  if (!createDirectories(directory))
    return {};
  return directory + "/LunaDash-" + QString::fromStdString(currentTimestamp()) +
         ".png";
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
  std::string error;
  if (!screenCapture_->selectRegion(clientEnvironment_, path.toStdString(),
                                    &error))
    captureError_ = QString::fromStdString(error);
}

void WaylandCompositor::closeTestSession(
    const std::function<void(bool)> &finished) {
  testStopping_ = true;
  for (const auto &client : clients_)
    closeClient(client.get());

  auto elapsed = std::make_shared<int>(0);
  auto poll = std::make_shared<std::function<void()>>();
  const std::weak_ptr<std::function<void()>> weakPoll = poll;
  *poll = [this, elapsed, finished, weakPoll] {
    *elapsed += 50;
    if (clients_.empty() && xwayland_)
      xwayland_->stop();
    const bool xwaylandDone = !xwayland_ || xwayland_->stopped();
    const bool processesDone = std::all_of(
        processes_.cbegin(), processes_.cend(), [](const auto &process) {
          return !process || !process->busy();
        });
    if ((clients_.empty() && processesDone && xwaylandDone) ||
        *elapsed >= 5000) {
      finished(clients_.empty() && processesDone && xwaylandDone &&
               !processFailure_);
      return;
    }
    if (const auto next = weakPoll.lock()) {
      if (!d->scheduleCallback(50, [next] { (*next)(); }))
        finished(false);
    }
  };
  if (!d->scheduleCallback(50, [poll] { (*poll)(); }))
    finished(false);
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
    terminateEventLoop(hasProcessFailure() ? 2 : 0);
}

} // namespace LunaDash
