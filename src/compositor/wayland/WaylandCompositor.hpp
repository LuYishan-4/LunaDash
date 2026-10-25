#pragma once

#include "compositor/layout/WindowLayout.hpp"
#include "compositor/window/animation/WindowAnimation.hpp"
#include "core/process/ProcessRunner.hpp"
#include "core/process/ProcessEnvironment.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <memory>
#include <string_view>
#include <vector>

namespace LunaDash {

class WindowSwitcher;
struct WindowTemplate;
class ScreenCapture;
class BrightnessSettings;
class DdcBrightnessSettings;
class ShellModules;
class AudioSettings;
class PowerSettings;
class SystemStatus;
class WeatherStatus;
class XWaylandSupport;
class NetworkStatus;
class PluginManager;
class ControlServer;
class SessionActions;
class SessionServices;
class ShortcutSettings;
class UpdateChecker;
struct ClientWindow;

class WaylandCompositor final {
public:
  WaylandCompositor(const std::string &socket, bool fullscreen, bool startShell,
                    const std::string &rendererPreference);
  ~WaylandCompositor();

  ProcessRunner *spawn(
      const std::vector<std::string> &arguments, const std::string &program = {},
      bool required = true,
      std::function<void(const ProcessResult &)> completion = {});
  bool saveScreenshot(const std::string &path,
                      const std::function<void(bool)> &finished = {});
  void saveState(const std::string &path);
  bool hasProcessFailure() const;
  int runEventLoop(bool profile = false);
  void terminateEventLoop(int exitCode);
  bool scheduleEventLoopCallback(int delayMilliseconds,
                                 std::function<void()> callback);
  void closeTestSession(const std::function<void(bool)> &finished);

private:
  class Impl;
  struct ThumbnailWriteState;
  std::unique_ptr<Impl> d;

  std::unique_ptr<WindowSwitcher> windowSwitcher_;
  ScreenCapture *screenCapture_ = nullptr;
  BrightnessSettings *brightnessSettings_ = nullptr;
  DdcBrightnessSettings *ddcBrightnessSettings_ = nullptr;
  ShellModules *shellModules_ = nullptr;
  AudioSettings *audioSettings_ = nullptr;
  PowerSettings *powerSettings_ = nullptr;
  std::unique_ptr<SessionActions> sessionActions_;
  std::unique_ptr<SessionServices> sessionServices_;
  ShortcutSettings *shortcutSettings_ = nullptr;
  UpdateChecker *updateChecker_ = nullptr;
  std::unique_ptr<SceneWindowAnimationTemplate> windowAnimations_;
  SystemStatus *systemStatus_ = nullptr;
  WeatherStatus *weatherStatus_ = nullptr;
  NetworkStatus *networkStatus_ = nullptr;
  XWaylandSupport *xwayland_ = nullptr;
  std::unique_ptr<PluginManager> pluginManager_;
  std::unique_ptr<ControlServer> controlServer_;

  std::string controlPath_;
  ProcessEnvironment clientEnvironment_;
  std::vector<std::unique_ptr<ClientWindow>> clients_;
  std::shared_ptr<ThumbnailWriteState> thumbnailWrite_;
  std::vector<std::shared_ptr<ProcessRunner>> processes_;
  int queuedThumbnailSerial_ = 0;
  std::vector<int> queuedThumbnailWindows_;
  ClientWindow *focused_ = nullptr;

  int nextWindowId_ = 1;
  int workspace_ = 0;
  int settingsSerial_ = 0;
  std::string settingsPage_ = "general";
  int pickerSerial_ = 0;
  int launcherSerial_ = 0;
  int shellActionSerial_ = 0;
  std::string shellAction_;
  int scratchpadWindow_ = 0;
  std::int64_t scratchpadProcess_ = 0;
  bool scratchpadPending_ = false;
  std::uint64_t scratchpadGeneration_ = 0;
  std::string scratchpadError_;
  bool launcherVisible_ = false;
  const WindowTemplate *windowTemplate_ = nullptr;
  std::unique_ptr<WindowLayout> windowLayout_;
  bool shuttingDown_ = false;
  bool logoutPending_ = false;
  bool testStopping_ = false;
  bool processFailure_ = false;
  bool shellRestartScheduled_ = false;
  int shellRestartFailures_ = 0;
  bool shortcutCapture_ = false;
  bool activationEnvironmentPublished_ = false;
  std::string activationEnvironmentError_;
  std::string lastCapture_;
  std::string captureError_;
  std::string screenshotDestination_;
  std::function<void(bool)> screenshotCompletion_;
  int modifierResends_ = 0;
  int keypadKeyForwards_ = 0;

  void requestShutdown();
  void scheduleShellRestart();
  void
  publishSessionActivationEnvironment(const std::function<void()> &ready = {});
  void captureScreen();
  bool launchExternalCommand(std::vector<std::string> command, std::string *error,
                             bool x11Helper = false);
  void resendKeyboardModifiers();
  void setLauncherVisible(bool visible, bool publish = true);
  std::string nextCapturePath() const;
  void configure(ClientWindow *client, const Rect &rectangle);
  void arrange();
  Rect workArea() const;
  Json currentWindowLayoutSettings() const;
  bool updateWindowLayoutSettings(const Json &changes, std::string *error);
  Json state(bool includeTranslations = true) const;
  Json control(const Json &request);
  // Diagnostics beyond state(): input focus, cursor, layers, helpers, frame timing.
  Json debugSnapshot() const;
  void focus(ClientWindow *client);
  void raiseWithDialogs(ClientWindow *client);
  void activateTask(int window);
  void beginWindowSwitch(int direction, bool workspaces = false);
  void selectWorkspace(int workspace);
  void publishWindowLayout();
  void captureWorkspaceThumbnail(int serial, std::vector<int> windows);
  void pollThumbnailWrite();
  void finishWindowSwitch(bool accept);
  void setMaximized(ClientWindow *client, bool maximized);
  void setFullscreen(ClientWindow *client, bool fullscreen);
  void focusNext(int direction);
  void synchronizeWindowFocus();
  void updateClientMetadata(ClientWindow *client);
  void removeClient(ClientWindow *client);
  void handleShortcut(std::string_view action);
  bool toggleScratchpad(std::string *error);
  void adoptScratchpad(ClientWindow *client);
  void applyKeyboardConfiguration();
};

} // namespace LunaDash
