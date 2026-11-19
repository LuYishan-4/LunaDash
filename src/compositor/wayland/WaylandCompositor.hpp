#pragma once

#include "compositor/commands/CommandRegistry.hpp"
#include "compositor/input/ShortcutRegistry.hpp"
#include "compositor/layout/WindowLayout.hpp"
#include "compositor/session/CompositorOptions.hpp"
#include "compositor/session/SessionHelpers.hpp"
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
struct wlr_scene_tree;

namespace LunaDash {

class WindowSwitcher;
class ScreenCapture;
class BrightnessSettings;
class DdcBrightnessSettings;
class AudioSettings;
class PowerSettings;
class XWaylandSupport;
class PluginManager;
class ControlServer;
class SessionActions;
class SessionServices;
class ShortcutSettings;
class VideoWallpaperPlayer;
struct ClientWindow;
struct CompositorCommands;
struct ShortcutActions;

class WaylandCompositor final {
public:
  // Creates the registries and desktop services; does not start wlroots.
  explicit WaylandCompositor(const CompositorOptions &options);
  ~WaylandCompositor();

  // Starts the wlroots runtime, the control socket, XWayland and the session
  // clients. False when the compositor cannot run; the reason is logged.
  bool start();

  ProcessRunner *spawn(
      const std::vector<std::string> &arguments, const std::string &program = {},
      bool required = true,
      std::function<void(const ProcessResult &)> completion = {});
  bool saveScreenshot(const std::string &path);
  void saveState(const std::string &path);
  bool hasProcessFailure() const;
  int runEventLoop(bool profile = false);
  void terminateEventLoop(int exitCode);
  bool scheduleEventLoopCallback(int delayMilliseconds,
                                 std::function<void()> callback);
  void closeTestSession(const std::function<void(bool)> &finished);
  // Asks the shell to open something, by compositor action name ("welcome",
  // "openControlCenter"); see sendShellRequest (compositor/session/ShellRequests.cpp).
  void requestShellAction(std::string_view action);
  // Runs `slintlu msg call Shell.request <action> <argument>`: the shell's own
  // IPC, the same any compositor's shortcut can use.
  void sendShellRequest(std::string_view action, std::string_view argument = {});
  // Publishes the workspaces over ext-workspace-v1; arrange() calls it.
  void publishWorkspaces();

  // Registration points. The compositor registers its built-ins at
  // construction; code built with the compositor adds its own entries here,
  // and only these registries decide what a name runs.
  CommandRegistry &commandRegistry() { return commands_; }
  ShortcutRegistry &shortcutRegistry() { return shortcuts_; }
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

  // Interactive (selection) and lunadashctl screenshot captures.
  struct CaptureState {
    std::string last;
    std::string error;
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
  struct ActivationEnvironmentState {
    bool published = false;
    std::string error;
  };

  // The workspace overview: every workspace zoomed out with live previews of
  // its windows, like niri's overview.
  struct OverviewState {
    struct Preview {
      int window = 0;
      int workspace = 0;
      Rect rect;
    };
    bool open = false;
    // 0 is the normal view, 1 fully zoomed out.
    double progress = 0.0;
    double velocity = 0.0;
    std::uint64_t lastStep = 0;
    wlr_scene_tree *tree = nullptr;
    std::vector<Preview> previews;
    // RGBA of the focus ring, read when the overview opens.
    float accent[4] = {0.61f, 0.80f, 0.98f, 1.0f};
    int hovered = 0;
    int pressed = 0;
    double pressX = 0.0;
    double pressY = 0.0;
    bool dragging = false;
    // A press that went to the shell: its motion and release go there too.
    bool shellPress = false;
    // Wheel distance not yet turned into a step, vertical and horizontal.
    double scroll = 0.0;
    double scrollHorizontal = 0.0;
    // The workspace shown in the middle: follows workspace_ on a spring, so
    // switching workspaces in the overview slides the column of workspaces.
    double view = 0.0;
    double viewVelocity = 0.0;
  };

  // Counters reported by state() for input diagnostics.
  struct KeyboardDiagnostics {
    int modifierResends = 0;
    int keypadKeyForwards = 0;
  };

  // Declared before d: the runtime reads them through Impl::options().
  const CompositorOptions options_;
  std::unique_ptr<Impl> d;

  CommandRegistry commands_;
  ShortcutRegistry shortcuts_;
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
  std::unique_ptr<SceneWindowAnimationTemplate> windowAnimations_;
  std::unique_ptr<XWaylandSupport> xwayland_;
  std::unique_ptr<PluginManager> pluginManager_;
  std::unique_ptr<ControlServer> controlServer_;
  // The last shellState() line sent to subscribers.
  std::string lastShellState_;
  bool shellStatePushScheduled_ = false;

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
  // The shell reported its launcher open (launcher-visible), for toggling it.
  bool launcherVisible_ = false;
  ActivationEnvironmentState activationEnvironment_;
  KeyboardDiagnostics keyboardDiagnostics_;
  OverviewState overview_;

  // Construction and start steps (WaylandCompositor.cpp).
  void registerBuiltins();
  void createDesktopServices();
  bool startControlServer();
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
  // The shell shows the page; it opens its first page for a name it does not know.
  void openSettingsPage(const std::string &page);
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
  Json state() const;
  // What a shell shows (workspaces' windows, wallpaper, preferences,
  // shortcuts), pushed to `subscribe` connections when it changes.
  Json shellState() const;
  // Pushes shellState() to subscribers if it changed; runs once per event-loop
  // pass however often it is asked for.
  void scheduleShellStatePush();
  void watchShellState();
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

  // Workspace overview (compositor/window/WorkspaceOverview.cpp).
  void toggleOverview();
  void closeOverview();
  bool overviewShown() const;
  bool overviewOpen() const;
  void handleOverviewShortcut(std::string_view action);
  // Steps the zoom to `presentNs` and redraws the previews; called every frame.
  void advanceOverview(std::uint64_t presentNs);
  // Pointer and key handling while the overview is shown; true when consumed.
  bool overviewPointerMotion();
  bool overviewPointerButton(std::uint32_t button, bool pressed);
  // A wheel step: vertical switches workspaces, horizontal (or Shift)
  // focuses the window beside the focused one.
  bool overviewAxis(double delta, bool horizontal);
  void rebuildOverviewPreviews();
  Rect overviewRect(const Rect &rectangle, int workspace) const;
  const OverviewState::Preview *overviewPreviewAt(double x, double y,
                                                  int except = 0) const;
  int overviewWorkspaceAt(double y) const;
  bool pointerOverShell() const;
  void moveClientToWorkspace(ClientWindow *client, int workspace);

  // Input (compositor/input/ShortcutActions.cpp).
  void handleShortcut(std::string_view action);
  void resendKeyboardModifiers();
  void applyKeyboardConfiguration();

  // Scratchpad (compositor/window/Scratchpad.cpp).
  bool toggleScratchpad(std::string *error);
  void adoptScratchpad(ClientWindow *client);
};

} // namespace LunaDash
