#pragma once

#include "compositor/commands/CommandRegistry.hpp"
#include "compositor/input/CompositorActions.hpp"
#include "compositor/ipc/ControlServer.hpp"
#include "compositor/layout/WindowLayout.hpp"
#include "compositor/session/CompositorOptions.hpp"
#include "compositor/session/SessionHelpers.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/animation/WindowAnimation.hpp"
#include "config/session/SessionConfig.hpp"
#include "core/process/ProcessEnvironment.hpp"
#include "core/process/ProcessRunner.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct wlr_layer_surface_v1;
struct wlr_scene_buffer;
struct wlr_scene_rect;
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
class SessionActions;
class SessionServices;
class KeyBindings;
class VideoWallpaperPlayer;
struct ClientWindow;
struct CompositorCommands;
struct CompositorActions;

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
  // Runs `slintlu msg call Shell.request <action> <argument>`: the shell's own
  // IPC, the same any compositor's shortcut can use.
  void sendShellRequest(std::string_view action, std::string_view argument = {});
  // Publishes the workspaces over ext-workspace-v1; arrange() calls it.
  void publishWorkspaces();


private:
  // IPC commands (compositor/commands) and actions (compositor/input/
  // CompositorActions.cpp, what keys are bound to) operate on the compositor state.
  friend struct CompositorCommands;
  friend struct CompositorActions;

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
    wlr_scene_rect *dim = nullptr;
    std::unordered_map<int, wlr_scene_tree *> previewTrees;
    // The focus or hover ring of each preview, kept just below it.
    std::unordered_map<int, wlr_scene_rect *> previewFrames;
    std::unordered_map<int, std::unordered_map<wlr_scene_buffer *, wlr_scene_buffer *>>
        previewBuffers;
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
  ActionRegistry actions_;
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
  std::unique_ptr<KeyBindings> keyBindings_;
  std::unique_ptr<SceneWindowAnimationTemplate> windowAnimations_;
  std::unique_ptr<XWaylandSupport> xwayland_;
  std::unique_ptr<PluginManager> pluginManager_;
  std::unique_ptr<ControlServer> controlServer_;
  // What `subscribe` connections were last told, by event (ipc/Events.cpp).
  std::map<std::string, Json, std::less<>> publishedEvents_;
  bool eventsScheduled_ = false;

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

  // What the get-* requests answer, one topic each, as sway's get_* requests
  // (compositor/ipc/StateSnapshot.cpp).
  Json describeVersion() const;
  Json describeClient(const ClientWindow &client) const;
  Json describeWorkspaces() const;
  Json describeLayout() const;
  Json describePreferences() const;
  Json describeAppearance() const;
  Json describeWallpaper() const;
  Json describeDevices() const;
  Json describeSession() const;
  Json describeRenderer() const;
  Json describeExtensions() const;
  Json describeSettings() const;
  // sway-style event subscriptions (compositor/ipc/Events.cpp).
  std::optional<ControlServer::Subscription>
  subscribe(std::string_view request);
  Json eventSnapshot(std::string_view event) const;
  bool hasSubscribers() const;
  // Sends each subscribed event what changed since it was last told.
  void publishEvents();
  // publishEvents() once per event-loop pass, however often it is asked for.
  void scheduleEvents();
  void watchEvents();
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

  // Runs an action with its arguments ("focus-workspace 2", "spawn kitty"),
  // as a key binding or `lunadashctl action` does (compositor/input/CompositorActions.cpp).
  bool runAction(std::string_view action, std::string *error = nullptr);
  // Keyboard (compositor/input/Input.cpp).
  void resendKeyboardModifiers();
  void applyKeyboardConfiguration();

  // Scratchpad (compositor/window/Scratchpad.cpp).
  bool toggleScratchpad(std::string *error);
  void adoptScratchpad(ClientWindow *client);
};

} // namespace LunaDash
