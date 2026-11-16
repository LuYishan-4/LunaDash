#include "compositor/wayland/WaylandCompositor.hpp"

#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/input/ShortcutActions.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/ipc/ControlServer.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/session/SessionActions.hpp"
#include "compositor/session/SessionEnvironment.hpp"
#include "compositor/session/SessionServices.hpp"
#include "compositor/session/SessionTuning.hpp"
#include "compositor/session/ShellLocation.hpp"
#include "compositor/session/ShellRendering.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/animation/AnimationProfile.hpp"
#include "compositor/window/animation/WindowAnimation.hpp"
#include "compositor/xwayland/XWaylandSupport.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/desktop/PreferenceKeys.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/json/JsonAccess.hpp"
#include "core/log/Log.hpp"
#include "core/process/ExecutableLookup.hpp"
#include "core/text/Url.hpp"
#include "desktop/audio/AudioSettings.hpp"
#include "desktop/display/BrightnessSettings.hpp"
#include "desktop/display/DdcBrightnessSettings.hpp"
#include "desktop/network/NetworkStatus.hpp"
#include "desktop/power/PowerSettings.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include "desktop/system/SystemStatus.hpp"
#include "desktop/system/UpdateChecker.hpp"
#include "desktop/wallpaper/VideoWallpaperPlayer.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "desktop/weather/WeatherStatus.hpp"

#include <glib.h>

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace LunaDash {

WaylandCompositor::WaylandCompositor(const std::string &socket, bool fullscreen,
                                     bool startShell,
                                     const std::string &rendererPreference)
    : d(std::make_unique<Impl>(this, socket, fullscreen, rendererPreference)) {
  registerBuiltins();
  windowTemplate_ = &defaultWindowTemplate();
  windowLayout_ = createWindowLayout(*windowTemplate_);
  createDesktopServices();
  installWindowPlacementFilter();

  if (!d->initialize())
    return;
  sessionServices_ = std::make_unique<SessionServices>(d->eventLoop);

  pluginManager_->setChangedCallback([this] {
    if (!d->scheduleCallback(0, [this] { arrange(); }))
      logWarning("Could not schedule a plugin change relayout.");
  });
  windowAnimations_ = std::make_unique<SceneWindowAnimationTemplate>();
  windowAnimations_->animation().setSnapshotLayer(d->animationLayer);
  windowAnimations_->configure(sceneAnimationOptions(
      windowAnimationProfile(*pluginManager_, desktopPreferences(), *windowTemplate_),
      windowTemplate_->motion));

  startControlServer(socket);

  ensureLegacyDesktopSettingsImported();
  clientEnvironment_ = createClientEnvironment(
      socket, controlPath_, executableDirectory(), loadDesktopPreferences());
  publishClientEnvironment(clientEnvironment_);

  startXWayland();

  // Portal backends are D-Bus activated. Do not start the shell/IME/apps until
  // the user bus and systemd manager know LunaDash's Wayland/display identity
  // and the portal frontend has been refreshed with that environment.
  publishSessionActivationEnvironment(
      [this, startShell] { startSessionClients(startShell); });

  logInfo("LunaDash compositor backend: wlroots socket: %s input: "
          "libinput/xkbcommon",
          socket.c_str());
}

void WaylandCompositor::registerBuiltins() {
  CompositorCommands::registerBuiltins(commands_);
  ShortcutActions::registerBuiltins(shortcuts_);
  registerBuiltinSettingsPages(settingsPages_);
  registerBuiltinSessionHelpers(sessionHelpers_);
  shellRequests_.settingsPage = defaultSettingsPage(settingsPages_);
}

void WaylandCompositor::createDesktopServices() {
  windowSwitcher_ = std::make_unique<WindowSwitcher>(d->eventLoop);
  videoWallpaper_ = std::make_unique<VideoWallpaperPlayer>(d->eventLoop);
  screenCapture_ = std::make_unique<ScreenCapture>(d->eventLoop);
  screenCapture_->setCompletion(
      [this](const std::string &path, const std::string &error) {
        onScreenCaptureFinished(path, error);
      });
  brightnessSettings_ = std::make_unique<BrightnessSettings>(d->eventLoop);
  ddcBrightnessSettings_ = std::make_unique<DdcBrightnessSettings>(d->eventLoop);
  systemStatus_ = std::make_unique<SystemStatus>(d->eventLoop);
  weatherStatus_ = std::make_unique<WeatherStatus>(d->eventLoop, [] {
    const auto preferences = desktopPreferences();
    return WeatherPreferences{
        preferenceValue(preferences, Preference::weatherEnabled),
        preferenceValue(preferences, Preference::weatherLatitude),
        preferenceValue(preferences, Preference::weatherLongitude),
        preferenceValue(preferences, Preference::weatherLocation)};
  });
  d->scheduleCallback(0, [this] {
    const auto path = wallpaperImagePath();
    if (!path.empty())
      refreshWallpaperPalette(d->eventLoop, path, wallpaperIsVideo(path));
  });
  audioSettings_ = std::make_unique<AudioSettings>(d->eventLoop);
  powerSettings_ = std::make_unique<PowerSettings>(d->eventLoop);
  sessionActions_ = std::make_unique<SessionActions>();
  shortcutSettings_ = std::make_unique<ShortcutSettings>();
  updateChecker_ = std::make_unique<UpdateChecker>(d->eventLoop, [] {
    return preferenceValue(desktopPreferences(), Preference::updateChannel);
  });
  networkStatus_ = std::make_unique<NetworkStatus>(d->eventLoop);
  pluginManager_ = std::make_unique<PluginManager>(d->eventLoop);
  pluginManager_->loadEnabled();
}

void WaylandCompositor::startControlServer(const std::string &socket) {
  controlPath_ = environmentValue("LUNADASH_CONTROL");
  if (controlPath_.empty())
    controlPath_ = environmentValue("LUDASH_CONTROL");
  if (controlPath_.empty())
    controlPath_ =
        std::string(g_get_user_runtime_dir()) + "/" + socket + "-control";
  windowSwitcher_->setChannelPath(controlPath_ + "-interaction.json");
  controlServer_ = std::make_unique<ControlServer>(
      d->eventLoop, controlPath_, [this](std::string_view request) {
        const auto document = parseJsonObject(request);
        const auto response = document.is_object()
                                  ? control(document)
                                  : Json{{"error", "Invalid request"}};
        return jsonCompact(response);
      });
}

void WaylandCompositor::startXWayland() {
  xwayland_ = std::make_unique<XWaylandSupport>();
  if (environmentInt("LUDASH_DISABLE_XWAYLAND") == 1)
    return;
  auto environment = clientEnvironment_;
  environment["XCURSOR_SIZE"] = std::to_string(
      preferenceValue(desktopPreferences(), Preference::cursorSize));
  if (!xwayland_->start(d->display, d->compositor, d->seat, environment,
                        [this](wlr_xwayland_surface *surface) {
                          if (d)
                            d->addXWaylandSurface(surface);
                        }))
    logWarning(
        "XWayland/XWM could not be prepared; X11 clients are unavailable.");
}

WaylandCompositor::~WaylandCompositor() {
  lifecycle_.shuttingDown = true;
  cancelWallpaperPalette();
  videoWallpaper_->stop();
  sessionServices_.reset();
  screenCapture_.reset();
  audioSettings_.reset();
  powerSettings_.reset();
  updateChecker_.reset();
  brightnessSettings_.reset();
  ddcBrightnessSettings_.reset();
  networkStatus_.reset();
  systemStatus_.reset();
  weatherStatus_.reset();
  if (windowAnimations_)
    windowAnimations_->clear();
  if (xwayland_) {
    xwayland_->stop();
    xwayland_.reset();
  }
  const auto childProcesses = processes_;
  processes_.clear();
  for (const auto &process : childProcesses) {
    if (!process)
      continue;
    process->cancel();
  }
  shortcutSettings_.reset();
  // Everything below owns sources on the compositor event loop. Release the
  // wlroots runtime first, then remove those sources while the loop is still
  // alive; destroying the display frees the loop.
  if (d)
    d->releaseRuntime();
  controlServer_.reset();
  pluginManager_.reset();
  windowSwitcher_.reset();
  thumbnailWrite_.reset();
  d.reset();
}

ProcessRunner *WaylandCompositor::spawn(
    const std::vector<std::string> &arguments, const std::string &program, bool required,
    std::function<void(const ProcessResult &)> completion) {
  auto environment = clientEnvironment_;
  applyXWaylandEnvironment(environment, xwayland_.get());

  const bool sessionShell =
      program.empty() &&
      std::find(arguments.cbegin(), arguments.cend(), "--session") !=
          arguments.cend();
  const bool welcome = arguments.size() >= 2 && arguments[0] == "--app" &&
                       arguments[1] == "welcome";
  std::vector<std::string> nativeArguments;
  std::string executable = program;
  if (sessionShell) {
    const auto shell = locateSessionShell();
    if (shell.empty()) {
      if (required)
        lifecycle_.processFailure = true;
      logWarning("The LunaDash shell (shell.slint) was not found in any search location.");
      return nullptr;
    }
    logInfo("LunaDash shell: %s", shell.c_str());
    const auto wallpaperPath = wallpaperImagePath();
    environment["LUNADASH_WALLPAPER"] =
        wallpaperPath.empty() ? std::string{} : localFileUri(wallpaperPath);
    const std::string slintlu = findSlintLu();
    if (slintlu.empty()) {
      if (required)
        lifecycle_.processFailure = true;
      logWarning("SlintLu is unavailable; install it to run the LunaDash shell.");
      return nullptr;
    }
    ShellRenderingEnvironment shellRenderingEnvironment;
    for (const auto *key : {"LUDASH_SHELL_RENDERER", "SLINTLU_RENDERER"}) {
      const auto value = environment.find(key);
      if (value != environment.end())
        shellRenderingEnvironment.emplace(key, value->second);
    }
    if (!configureSlintLuRendering(shellRenderingEnvironment)) {
      logWarning(
          "LUDASH_SHELL_RENDERER must be auto, opengl, vulkan, or software.");
      lifecycle_.processFailure = true;
      return nullptr;
    }
    for (const auto &[key, value] : shellRenderingEnvironment)
      environment[key] = value;
    executable = slintlu;
    nativeArguments = {"--config", shell};
  } else {
    if (executable.empty())
      executable = bundledExecutable(welcome ? "welcome" : "desktop");
    if (!welcome)
      nativeArguments = arguments;
  }

  if (executable.empty()) {
    if (required)
      lifecycle_.processFailure = true;
    return nullptr;
  }

  auto process = std::make_shared<ProcessRunner>(d->eventLoop, false, true);
  auto *raw = process.get();
  processes_.push_back(process);
  const std::weak_ptr<ProcessRunner> weakProcess = process;
  const bool started = process->runDetailed(
      executable, nativeArguments,
      [this, raw, weakProcess, required, sessionShell,
       completion = std::move(completion)](const ProcessResult &result) {
        if (!lifecycle_.shuttingDown && !result.success) {
          if (required)
            lifecycle_.processFailure = true;
          logWarning("LunaDash child process exited unsuccessfully.");
        }
        if (completion)
          completion(result);
        if (sessionShell && !lifecycle_.shuttingDown &&
            !lifecycle_.testStopping && !lifecycle_.logoutPending) {
          logWarning("LunaDash shell exited; scheduling restart.");
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
      lifecycle_.processFailure = true;
    return nullptr;
  }
  if (sessionShell)
    d->scheduleCallback(SessionTuning::shellHealthyAfterMs, [this, weakProcess] {
      if (const auto active = weakProcess.lock(); active && active->busy())
        lifecycle_.shellRestartFailures = 0;
    });
  return raw;
}

void WaylandCompositor::saveState(const std::string &path) {
  std::ofstream file(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
  if (!file)
    return;
  const auto bytes = jsonIndented(state());
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool WaylandCompositor::hasProcessFailure() const {
  return lifecycle_.processFailure;
}

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

} // namespace LunaDash
