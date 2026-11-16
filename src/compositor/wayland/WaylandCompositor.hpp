#pragma once

#include "compositor/commands/CommandRegistry.hpp"
#include "compositor/input/ShortcutRegistry.hpp"
#include "compositor/layout/WindowLayout.hpp"
#include "compositor/session/SessionHelpers.hpp"
#include "compositor/settings/SettingsPages.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/animation/WindowAnimation.hpp"
#include "config/session/SessionConfig.hpp"
#include "core/process/ProcessRunner.hpp"
#include "core/process/ProcessEnvironment.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <memory>
#include <string_view>
#include <vector>

struct wlr_layer_surface_v1;

namespace LunaDash {

class WindowSwitcher;
class ScreenCapture;
class BrightnessSettings;
class DdcBrightnessSettings;
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
class VideoWallpaperPlayer;
struct ClientWindow;
struct CompositorCommands;
struct ShortcutActions;

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

  // Registration points. The compositor registers its built-ins at
  // construction; code built with the compositor adds its own entries here,
  // and only these registries decide what a name runs.
  CommandRegistry &commandRegistry() { return commands_; }
  ShortcutRegistry &shortcutRegistry() { return shortcuts_; }
  SettingsPageRegistry &settingsPageRegistry() { return settingsPages_; }
  SessionHelperRegistry &sessionHelperRegistry() { return sessionHelpers_; }

private:
  // IPC commands (compositor/commands) and keyboard shortcuts
  // (compositor/input/ShortcutActions.cpp) operate on the compositor state.
  friend struct CompositorCommands;
  friend struct ShortcutActions;

  class Impl;
  struct ThumbnailWriteState;

  // The scratchpad terminal: the window once mapped, or the pending launch.
  struct ScratchpadState {
    int window = 0;
    std::int64_t process = 0;
    bool pending = false;
    std::uint64_t generation = 0;
    std::string error;
  };

  // Interactive (selection) and test (saveScreenshot) captures.
  struct CaptureState {
    std::string last;
    std::string error;
    std::string destination;
    std::function<void(bool)> completion;
  };

  // Compositor lifetime and the supervised session shell.
  struct LifecycleState {
    bool shuttingDown = false;
    bool logoutPending = false;
    bool testStopping = false;
    bool processFailure = false;
    bool shellRestartScheduled = false;
    int shellRestartFailures = 0;
  };

  // Requests the shell picks up from state(): each serial changes once per request.
  struct ShellRequestState {
    int settingsSerial = 0;
    std::string settingsPage;
    int pickerSerial = 0;
    int launcherSerial = 0;
    bool launcherVisible = false;
    int actionSerial = 0;
    std::string action;
  };

  struct ActivationEnvironmentState {
    bool published = false;
    std::string error;
  };

  // Counters reported by state() for input diagnostics.
  struct KeyboardDiagnostics {
    int modifierResends = 0;
    int keypadKeyForwards = 0;
  };

  std::unique_ptr<Impl> d;

  CommandRegistry commands_;
  ShortcutRegistry shortcuts_;
  SettingsPageRegistry settingsPages_;
  SessionHelperRegistry sessionHelpers_;

  std::unique_ptr<WindowSwitcher> windowSwitcher_;
  std::unique_ptr<VideoWallpaperPlayer> videoWallpaper_;
  std::unique_ptr<ScreenCapture> screenCapture_;
  std::unique_ptr<BrightnessSettings> brightnessSettings_;
  std::unique_ptr<DdcBrightnessSettings> ddcBrightnessSettings_;
  std::unique_ptr<AudioSettings> audioSettings_;
  std::unique_ptr<PowerSettings> powerSettings_;
  std::unique_ptr<SessionActions> sessionActions_;
  std::unique_ptr<SessionServices> sessionServices_;
  std::unique_ptr<ShortcutSettings> shortcutSettings_;
  std::unique_ptr<UpdateChecker> updateChecker_;
  std::unique_ptr<SceneWindowAnimationTemplate> windowAnimations_;
  std::unique_ptr<SystemStatus> systemStatus_;
  std::unique_ptr<WeatherStatus> weatherStatus_;
  std::unique_ptr<NetworkStatus> networkStatus_;
  std::unique_ptr<XWaylandSupport> xwayland_;
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
  // Direction of a pending workspace change (1 next, -1 previous) for the next arrange().
  int workspaceSlide_ = 0;
  // The shell, launcher and autostart commands from session.json.
  SessionConfig session_;
  const WindowTemplate *windowTemplate_ = nullptr;
  std::unique_ptr<WindowLayout> windowLayout_;
  bool shortcutCapture_ = false;

  ScratchpadState scratchpad_;
  CaptureState capture_;
  LifecycleState lifecycle_;
  ShellRequestState shellRequests_;
  ActivationEnvironmentState activationEnvironment_;
  KeyboardDiagnostics keyboardDiagnostics_;

  // Construction steps (WaylandCompositor.cpp).
  void registerBuiltins();
  void createDesktopServices();
  void startControlServer(const std::string &socket);
  void startXWayland();

  // Session lifetime (compositor/session/SessionStartup.cpp).
  void startSessionClients(bool startShell);
  void requestShutdown();
  void scheduleShellRestart();
  void
  publishSessionActivationEnvironment(const std::function<void()> &ready = {});
  bool launchExternalCommand(std::vector<std::string> command, std::string *error,
                             bool x11Helper = false);

  // Shell requests (compositor/session/ShellRequests.cpp).
  void setLauncherVisible(bool visible, bool publish = true);
  void requestShellAction(std::string_view action);
  bool openSettingsPage(const std::string &page, std::string *error);
  bool openWallpaperPicker(std::string *error);

  // Screenshots (compositor/capture/ScreenshotRequests.cpp).
  void captureScreen();
  void onScreenCaptureFinished(const std::string &path, const std::string &error);
  std::string nextCapturePath() const;

  // Window layout (compositor/window/Arrange.cpp, WindowLayoutSettings.cpp).
  void configure(ClientWindow *client, const Rect &rectangle);
  void arrange();
  // Applies opacity, blur and eye-care preferences to the window effects.
  void applyVisualPreferences(const Json &preferences);
  // Switches to the selected window template; true when it changed.
  bool selectWindowTemplate();
  void installWindowPlacementFilter();
  Rect floatingGeometry(const ClientWindow &client, const Rect &area) const;
  Rect workArea() const;
  Json currentWindowLayoutSettings() const;
  // The window template in use: a layout plugin's, or tiling in its fill or
  // scrolling mode.
  std::string selectedWindowTemplateKey() const;
  bool updateWindowLayoutSettings(const Json &changes, std::string *error);
  bool performLayoutAction(const std::string &action, const Json &payload);

  // Status for the shell and tools (compositor/ipc/StateSnapshot.cpp).
  Json state(bool includeTranslations = true) const;
  Json describeClients(Json &xwaylandUtilities) const;
  Json describeTiling() const;
  Json describeXWayland(Json utilitySurfaces) const;
  Json describeInput(const Json &preferences) const;
  Json describeWallpaperState() const;
  // IPC requests (compositor/commands/CommandTable.cpp).
  Json control(const Json &request);
  // Diagnostics beyond state(): input focus, cursor, layers, helpers, frame timing.
  Json debugSnapshot() const;

  // Window focus and state (compositor/window/WindowFocus.cpp).
  void focus(ClientWindow *client);
  void raiseWithDialogs(ClientWindow *client);
  void setMaximized(ClientWindow *client, bool maximized);
  void setFullscreen(ClientWindow *client, bool fullscreen);
  void minimizeClient(ClientWindow *client);
  void focusNext(int direction);
  void synchronizeWindowFocus();
  void updateClientMetadata(ClientWindow *client);
  void removeClient(ClientWindow *client);
  ClientWindow *clientById(int id) const;

  // Window switcher and workspaces (compositor/window/WindowSwitch.cpp).
  void activateTask(int window);
  void beginWindowSwitch(int direction, bool workspaces = false);
  void selectWorkspace(int workspace);
  // Workspaces follow the windows: the occupied ones plus one empty workspace
  // after them (and the current one while it is still shown).
  int workspaceCount() const;
  // Removes empty workspaces between occupied ones, except the current one.
  void compactWorkspaces();
  // The video wallpaper player's surfaces stay below the shell's wallpaper.
  bool videoWallpaperSurface(const wlr_layer_surface_v1 *surface) const;
  void publishWindowLayout();
  void captureWorkspaceThumbnail(int serial, std::vector<int> windows);
  void pollThumbnailWrite();
  void finishWindowSwitch(bool accept);

  // Input (compositor/input/ShortcutActions.cpp).
  void handleShortcut(std::string_view action);
  void resendKeyboardModifiers();
  void applyKeyboardConfiguration();

  // Scratchpad (compositor/window/Scratchpad.cpp).
  bool toggleScratchpad(std::string *error);
  void adoptScratchpad(ClientWindow *client);
};

} // namespace LunaDash
