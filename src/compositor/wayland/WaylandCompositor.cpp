#include "compositor/wayland/WaylandCompositor.hpp"
#include "core/log/StallTrace.hpp"

#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/input/CompositorActions.hpp"
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
#include "config/BuildConfig.hpp"
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
#include "desktop/power/PowerSettings.hpp"
#include "desktop/shortcuts/KeyBindings.hpp"
#include "desktop/wallpaper/VideoWallpaperPlayer.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"

#include <glib.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace LunaDash {

WaylandCompositor::WaylandCompositor(const CompositorOptions &options)
    : options_(options), d(std::make_unique<Impl>(this)) {
  registerBuiltins();
  windowTemplate_ = &defaultWindowTemplate();
  windowLayout_ = createWindowLayout(*windowTemplate_);
  createDesktopServices();
  installWindowPlacementFilter();
}

bool WaylandCompositor::start() {
  std::error_code executablePathError;
  const auto executablePath =
      std::filesystem::read_symlink("/proc/self/exe", executablePathError);
  logInfo("LunaDash version: %s (%s) executable: %s", BuildConfig::version,
          BuildConfig::commit, executablePath.string().c_str());

  if (!d->initialize())
    return false;
  sessionServices_ = std::make_unique<SessionServices>(d->eventLoop);
  if (WorkspaceProtocol::supported())
    d->workspaceProtocol = std::make_unique<WorkspaceProtocol>(
        d->display, [this](int index) { selectWorkspace(index); });
  d->idleNotify = std::make_unique<IdleNotify>(d->display, d->idleInhibit, d->seat);
  d->sessionLock = std::make_unique<SessionLock>(
      d->display, &d->scene->tree,
      SessionLock::Callbacks{
          [this](wlr_surface *surface) {
            if (surface)
              d->focusSurface(surface);
            else
              wlr_seat_keyboard_notify_clear_focus(d->seat);
          },
          [this](bool locked) { d->sessionLockChanged(locked); }});
  d->updateSessionLockOutputs();

  pluginManager_->setChangedCallback([this] {
    if (!d->scheduleCallback(0, [this] { arrange(); }))
      logWarning("Could not schedule a plugin change relayout.");
  });
  windowAnimations_ = std::make_unique<SceneWindowAnimationTemplate>();
  windowAnimations_->animation().setSnapshotLayer(d->animationLayer);
  windowAnimations_->animation().setResizeHook(
      [this](wlr_scene_tree *tree, const SceneAnimationRectangle *current) {
        for (const auto &client : clients_)
          if (client->sceneTree == tree) {
            client->animatedSize =
                current ? Size(current->width, current->height) : Size();
            return;
          }
      });
  windowAnimations_->configure(sceneAnimationOptions(
      windowAnimationProfile(*pluginManager_, desktopPreferences(), *windowTemplate_),
      windowTemplate_->motion));

  if (!startControlServer())
    return false;
  watchShellState();

  ensureLegacyDesktopSettingsImported();
  clientEnvironment_ = createClientEnvironment(
      options_.socket, controlPath_, executableDirectory(),
      loadDesktopPreferences());
  publishClientEnvironment(clientEnvironment_);

  startXWayland();

  publishSessionActivationEnvironment(
      [this] { startSessionClients(!options_.noShell); });

  logInfo("LunaDash compositor backend: wlroots socket: %s input: "
          "libinput/xkbcommon",
          options_.socket.c_str());
  return true;
}

void WaylandCompositor::registerBuiltins() {
  CompositorCommands::registerBuiltins(commands_);
  CompositorActions::registerBuiltins(actions_);
  registerBuiltinSessionHelpers(sessionHelpers_);
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
  d->scheduleCallback(0, [this] {
    const auto path = wallpaperImagePath();
    if (!path.empty())
      refreshWallpaperPalette(d->eventLoop, path, wallpaperIsVideo(path));
  });
  audioSettings_ = std::make_unique<AudioSettings>(d->eventLoop);
  powerSettings_ = std::make_unique<PowerSettings>(d->eventLoop);
  sessionActions_ = std::make_unique<SessionActions>();
  // Bindings name actions with their arguments; only registered ones bind.
  keyBindings_ = std::make_unique<KeyBindings>(
      [this](std::string_view action, std::string *error) {
        const auto call = parseActionCall(action, error);
        return call && checkActionCall(actions_, *call, error);
      });
  pluginManager_ = std::make_unique<PluginManager>(d->eventLoop);
  pluginManager_->loadEnabled();
}

bool WaylandCompositor::startControlServer() {
  controlPath_ = environmentValue("LUNADASH_CONTROL");
  if (controlPath_.empty())
    controlPath_ =
        std::string(g_get_user_runtime_dir()) + "/" + options_.socket + "-control";
  windowSwitcher_->setChannelPath(controlPath_ + "-interaction.json");
  try {
    controlServer_ = std::make_unique<ControlServer>(
        d->eventLoop, controlPath_,
        [this](std::string_view request) {
          const auto document = parseJsonObject(request);
          StallTrace::Section trace("command", jsonString(document, "method"));
          const auto response = document.is_object()
                                    ? control(document)
                                    : Json{{"error", "Invalid request"}};
          // A command may have changed what the shell shows.
          scheduleShellStatePush();
          return jsonCompact(response);
        },
        // {"method": "subscribe"}: shellState() now and whenever it changes,
        // instead of polling the full status.
        [this](std::string_view request) -> std::optional<std::string> {
          const auto document = parseJsonObject(request);
          if (!document.is_object() || jsonString(document, "method") != "subscribe")
            return std::nullopt;
          lastShellState_ = jsonCompact(shellState());
          return lastShellState_;
        });
  } catch (const std::exception &error) {
    logCritical("LunaDash control socket %s: %s", controlPath_.c_str(),
                error.what());
    lifecycle_.processFailure = true;
    return false;
  }
  return true;
}

// Changes that pass neither arrange() nor a command (a palette finished in the
// background, a preferences file edited by hand) are found by a slow check.
void WaylandCompositor::watchShellState() {
  if (!d)
    return;
  d->scheduleCallback(2000, [this] {
    if (lifecycle_.shuttingDown)
      return;
    scheduleShellStatePush();
    watchShellState();
  });
}

void WaylandCompositor::scheduleShellStatePush() {
  if (shellStatePushScheduled_ || !controlServer_ || !d ||
      controlServer_->subscribers() == 0)
    return;
  shellStatePushScheduled_ = true;
  d->scheduleCallback(0, [this] {
    shellStatePushScheduled_ = false;
    if (!controlServer_ || controlServer_->subscribers() == 0)
      return;
    auto line = jsonCompact(shellState());
    if (line == lastShellState_)
      return;
    lastShellState_ = std::move(line);
    controlServer_->broadcast(lastShellState_);
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
  // The handles belong to the manager, which goes with the display.
  if (d)
    d->foreignToplevels.reset();
  cancelWallpaperPalette();
  videoWallpaper_->stop();
  sessionServices_.reset();
  screenCapture_.reset();
  audioSettings_.reset();
  powerSettings_.reset();
  brightnessSettings_.reset();
  ddcBrightnessSettings_.reset();
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
  keyBindings_.reset();
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
      executable = bundledExecutable("desktop");
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
