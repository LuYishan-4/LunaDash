#pragma once

#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/input/PointerCursor.hpp"
#include "compositor/wayland/BackgroundEffectManager.hpp"
#include "compositor/wayland/ForeignToplevels.hpp"
#include "compositor/wayland/IdleNotify.hpp"
#include "compositor/wayland/SessionLock.hpp"
#include "compositor/wayland/WorkspaceProtocol.hpp"
#include "compositor/renderer/blur/WindowGlass.hpp"
#include "compositor/renderer/clip/WindowCorners.hpp"
#include "compositor/renderer/color/NightColor.h"
#include "compositor/session/SessionSplash.hpp"
#include "compositor/window/animation/SceneAnimation.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "core/templates/LunaClint.hpp"
#include "core/templates/WaylandSlot.hpp"
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>
#include <wayland-server-core.h>


struct wlr_xwayland_surface_configure_event;
struct wlr_xwayland_minimize_event;

namespace LunaDash {
using Templates::LunaClint;
using Templates::WaylandSlot;
inline constexpr int kDefaultOutputRefreshMilliHz = 60000;

class WaylandCompositor::Impl {
public:
  using Slot = WaylandSlot;

  class Component {
  public:
    explicit Component(Impl *owner) : impl(owner) {}
    virtual ~Component() = default;
    Component(const Component &) = delete;
    Component &operator=(const Component &) = delete;
    virtual bool start() = 0;

  protected:
    Impl *const impl;
  };
  class OutputComponent;
  class XdgShellComponent;
  struct DecorationState;
  class LayerShellComponent;
  class WindowCaptureComponent;
  class InputDeviceComponent;
  class PointerComponent;
  class TextInputComponent;
  class ClipboardComponent;

  struct OutputState : LunaClint<OutputState> {
    OutputState(Impl *owner, wlr_output *wlrOutput);
    ~OutputState();
    OutputState(const OutputState &) = delete;
    OutputState &operator=(const OutputState &) = delete;

    void onFrame();
    void onRequestState(wlr_output_event_request_state *event);
    void onDestroy();
    void onScreencopyTimer();

    Impl *const impl;
    wlr_output *const output;
    wlr_scene_output *sceneOutput = nullptr;
    int screencopyKeepalive = 0;
    wl_event_source *screencopyTimer = nullptr;
    bool screencopyTimerScheduled = false;
    bool softwareCursorLocked = false;
    bool forceSoftwareCursor = false;
    std::uint64_t lastAnimationPublishNs = 0;
    // Windows on no output get a frame callback this often, not every frame.
    static constexpr std::uint64_t kHiddenFrameIntervalNs = 1000000000ULL;
    std::uint64_t lastHiddenFrameDoneNs = 0;
    std::uint64_t frameCallbacks = 0;
    std::uint64_t slowFrames = 0;
    std::int64_t lastFrameWorkUsec = 0;
    std::int64_t maxFrameWorkUsec = 0;
    // Where a frame's work goes (lunadashctl debug outputs): animations,
    // overview, glass, content scale, scene commit, frame done.
    struct FrameStage {
      std::int64_t lastUsec = 0;
      std::int64_t maxUsec = 0;
      std::uint64_t slow = 0;
    };
    static constexpr const char *frameStageNames[] = {
        "animations", "overview", "glass", "contentScale", "commit", "frameDone"};
    FrameStage frameStages[std::size(frameStageNames)];
    ludash_night_color *nightColor = nullptr;
    int nightTemperature = 6500;
    std::string nightError;
    Slot frame;
    Slot destroy;
    Slot requestState;

  private:
    bool updateCaptureCursor();
    void advanceAnimations(std::uint64_t nowNs);
    std::uint64_t refreshPeriodNs() const;
    void commitScene();
    void sendFrameDone(timespec *now);
    void scheduleCaptureTail(bool capturePending);
    void recordFrameWork(std::uint64_t startedNs);
  };

  struct ToplevelState : LunaClint<ToplevelState> {
    ToplevelState(Impl *owner, ClientWindow *window);
    ~ToplevelState();
    ToplevelState(const ToplevelState &) = delete;
    ToplevelState &operator=(const ToplevelState &) = delete;
    void listenTo(wlr_xdg_surface *surface, wlr_xdg_toplevel *toplevel);

    void onMap();
    void onUnmap();
    void onCommit();
    void onMetadata();
    void onParent();
    void onMinimize();
    void onMaximize();
    void onFullscreen();
    void onMove(wlr_xdg_toplevel_move_event *event);
    void onResize(wlr_xdg_toplevel_resize_event *event);
    void onSceneContentDestroy();
    void onDestroy();

#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
    void createForeignToplevel();
    void updateForeignToplevel();
    void destroyForeignToplevel();
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
    void createLegacyForeignToplevel();
    void updateLegacyForeignToplevel();
    void destroyLegacyForeignToplevel();
#endif
    // Foreign-toplevel handles and the capture scene; safe to call twice.
    void releaseProtocolObjects();

    Impl *const impl;
    ClientWindow *const client;
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
    wlr_scene *imageCaptureScene = nullptr;
    wlr_ext_foreign_toplevel_handle_v1 *foreignHandle = nullptr;
    wlr_ext_image_capture_source_v1 *imageCaptureSource = nullptr;
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
#endif
    bool hasSurfaceGeometry = false;
    wlr_box lastSurfaceGeometry{};
    Slot map;
    Slot unmap;
    Slot commit;
    Slot destroy;
    Slot sceneContentDestroy;
    Slot setTitle;
    Slot setAppId;
    Slot setParent;
    Slot requestMinimize;
    Slot requestMaximize;
    Slot requestFullscreen;
    Slot requestMove;
    Slot requestResize;
  };

  struct XWaylandState : LunaClint<XWaylandState> {
    XWaylandState(Impl *owner, ClientWindow *window);
    XWaylandState(const XWaylandState &) = delete;
    XWaylandState &operator=(const XWaylandState &) = delete;
    void listenToSurface();

    void onAssociate();
    void onDissociate();
    void onMap();
    void onUnmap();
    void onConfigure(wlr_xwayland_surface_configure_event *event);
    void onMove();
    void onResize();
    void onMinimize(wlr_xwayland_minimize_event *event);
    void onMaximize();
    void onFullscreen();
    void onActivate();
    void onMetadata();
#if WLR_VERSION_MINOR >= 20
    void onIcon();
#endif
    void onSceneDestroy();
    void onSceneContentDestroy();
    void onDestroy();

    Impl *const impl;
    ClientWindow *const client;
#if WLR_VERSION_MINOR >= 20
    Slot setIcon;
#endif
    Slot associate;
    Slot dissociate;
    Slot map;
    Slot unmap;
    Slot sceneDestroy;
    Slot sceneContentDestroy;
    Slot destroy;
    Slot requestConfigure;
    Slot requestMove;
    Slot requestResize;
    Slot requestMinimize;
    Slot requestMaximize;
    Slot requestFullscreen;
    Slot requestActivate;
    Slot setTitle;
    Slot setClass;
    Slot setParent;
    Slot setGeometry;
    Slot setOverrideRedirect;
  };

  struct DeferredClientRemoval {
    void onTimer();
    Impl *impl = nullptr;
    ClientWindow *client = nullptr;
  };

  struct ScheduledCallback {
    void onTimer();
    Impl *impl = nullptr;
    wl_event_source *source = nullptr;
    std::function<void()> callback;
  };

  struct LayerState : LunaClint<LayerState> {
    LayerState(Impl *owner, wlr_layer_surface_v1 *layerSurface,
               wlr_scene_layer_surface_v1 *scene);
    LayerState(const LayerState &) = delete;
    LayerState &operator=(const LayerState &) = delete;

    void onMap();
    void onUnmap();
    void onCommit();
    void onDestroy();

    Impl *const impl;
    wlr_layer_surface_v1 *const surface;
    wlr_scene_layer_surface_v1 *const sceneLayer;
    bool mapped = false;
    bool configured = false;
    Slot map;
    Slot unmap;
    Slot commit;
    Slot destroy;
  };

  struct XdgPopupState : LunaClint<XdgPopupState> {
    XdgPopupState(Impl *owner, wlr_xdg_popup *xdgPopup);
    XdgPopupState(const XdgPopupState &) = delete;
    XdgPopupState &operator=(const XdgPopupState &) = delete;

    void onCommit();
    void onReposition();
    void onDestroy();

    Impl *const impl;
    wlr_xdg_popup *const popup;
    wlr_scene_tree *sceneTree = nullptr;
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
    wlr_scene_tree *captureTree = nullptr;
#endif
    Slot commit;
    Slot reposition;
    Slot destroy;
  };

  struct KeyboardState : LunaClint<KeyboardState> {
    KeyboardState(Impl *owner, wlr_keyboard *wlrKeyboard, bool isVirtual,
                  wl_client *owningClient);
    KeyboardState(const KeyboardState &) = delete;
    KeyboardState &operator=(const KeyboardState &) = delete;
    void listenForKeymap();

    void onKey(wlr_keyboard_key_event *event);
    void onModifiers();
    void onKeymap();
    void onDestroy();

    Impl *const impl;
    wlr_keyboard *const keyboard;
    wl_client *const ownerClient;
    const bool virtualKeyboard;
    uint32_t lastLockedModifiers = 0;
    std::unordered_set<uint32_t> consumedKeys;
    bool metaTapPending = false;
    int metaTapKeycode = -1;
    Slot key;
    Slot keymap;
    Slot modifiers;
    Slot destroy;

  private:
    bool inputMethodBridgeActive() const;
  };

  struct InputMethodState : LunaClint<InputMethodState> {
    InputMethodState(Impl *owner, wlr_input_method_v2 *inputMethod);
    InputMethodState(const InputMethodState &) = delete;
    InputMethodState &operator=(const InputMethodState &) = delete;

    void onCommit();
    void onNewPopup(wlr_input_popup_surface_v2 *popup);
    void onGrabKeyboard(wlr_input_method_keyboard_grab_v2 *grab);
    void onDestroy();

    Impl *const impl;
    wlr_input_method_v2 *const method;
    Slot commit;
    Slot newPopup;
    Slot grabKeyboard;
    Slot destroy;
  };

  struct TextInputState : LunaClint<TextInputState> {
    TextInputState(Impl *owner, wlr_text_input_v3 *textInput);
    TextInputState(const TextInputState &) = delete;
    TextInputState &operator=(const TextInputState &) = delete;

    void onEnable();
    void onCommit();
    void onDisable();
    void onDestroy();

    Impl *const impl;
    wlr_text_input_v3 *const text;
    Slot enable;
    Slot commit;
    Slot disable;
    Slot destroy;
  };

  struct PopupState : LunaClint<PopupState> {
    PopupState(Impl *owner, wlr_input_popup_surface_v2 *inputPopup,
               wlr_scene_tree *tree);
    PopupState(const PopupState &) = delete;
    PopupState &operator=(const PopupState &) = delete;

    void onDestroy();

    Impl *const impl;
    wlr_input_popup_surface_v2 *const popup;
    wlr_scene_tree *const sceneTree;
    Slot destroy;
  };

  WaylandCompositor *q = nullptr;
  // True when another Wayland or X11 display hosts this compositor.
  bool nested = false;
  // The command-line options, owned by the compositor.
  const CompositorOptions &options() const { return q->options_; }

  wl_display *display = nullptr;
  wl_event_loop *eventLoop = nullptr;
  wlr_backend *backend = nullptr;
  wlr_compositor *compositor = nullptr;
  wlr_renderer *renderer = nullptr;
  wlr_allocator *allocator = nullptr;
  wlr_output_layout *outputLayout = nullptr;
  wlr_scene *scene = nullptr;
  std::unique_ptr<WindowGlass> windowGlass;
  std::unique_ptr<WindowCorners> windowCorners;
  wlr_scene_output_layout *sceneLayout = nullptr;
  wlr_scene_tree *backgroundLayer = nullptr;
  wlr_scene_tree *bottomLayer = nullptr;
  wlr_scene_tree *normalLayer = nullptr;
  wlr_scene_tree *animationLayer = nullptr;
  // Window previews of the workspace overview, below the shell's panels.
  wlr_scene_tree *overviewLayer = nullptr;
  wlr_scene_tree *topLayer = nullptr;
  wlr_scene_tree *fullscreenLayer = nullptr;
  // Black under a fullscreen window, where one that does not fill the screen
  // would show what is behind it; shown while one is on the workspace.
  wlr_scene_rect *fullscreenBackdrop = nullptr;
  wlr_scene_tree *overlayLayer = nullptr;
  // The session splash, above everything while the shell starts.
  wlr_scene_tree *splashLayer = nullptr;
  std::unique_ptr<SessionSplash> splash;
  wlr_scene_rect *background = nullptr;
  static constexpr float backgroundColor[4] = {0.043f, 0.067f, 0.078f, 1.0f};
  wlr_output *primaryOutput = nullptr;
  wlr_output *pendingDisplay = nullptr;
  wlr_output_state previousDisplay{};
  wl_event_source *displayRevertTimer = nullptr;
  std::chrono::steady_clock::time_point displayRevertDeadline{};
  std::string displayError;
  // Desktop preferences virtualOutputSize and virtualOutputSizes; read once
  // and again after preferences change, never per frame or layout.
  Size virtualOutputSize;
  std::vector<Size> virtualOutputSizes;
  Rect usableArea;
  bool eyeCareActive = false;
  float steadyWindowOpacity = 0.90f;
  std::uint64_t dispatchCalls = 0;
  std::uint64_t slowDispatches = 0;
  std::int64_t lastDispatchUsec = 0;
  std::int64_t maxDispatchUsec = 0;
  int exitCode = 0;
  bool eventLoopRunning = true;

  wlr_xdg_shell *xdgShell = nullptr;
  wlr_layer_shell_v1 *layerShell = nullptr;
#if LUDASH_WLR_HAS_XDG_ACTIVATION
  wlr_xdg_activation_v1 *xdgActivation = nullptr;
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  wlr_foreign_toplevel_manager_v1 *foreignToplevelManager = nullptr;
#endif
  wlr_seat *seat = nullptr;
  wlr_cursor *cursor = nullptr;
  wlr_xcursor_manager *cursorManager = nullptr;
  // The image on `cursor`, grown while the pointer is shaken.
  std::unique_ptr<PointerCursor> pointerCursor;
  wlr_cursor_shape_manager_v1 *cursorShapeManager = nullptr;
  std::unique_ptr<BackgroundEffectManager> backgroundEffects;
  // ext-workspace-v1 (updated by WaylandCompositor::arrange).
  std::unique_ptr<WorkspaceProtocol> workspaceProtocol;
  // ext-idle-notify-v1; input reports activity to it.
  std::unique_ptr<IdleNotify> idleNotify;
  // wlr-foreign-toplevel handles for every window, with their requests.
  std::unique_ptr<ForeignToplevels> foreignToplevels;
  // ext-session-lock-v1. While locked, input goes only to lock surfaces.
  std::unique_ptr<SessionLock> sessionLock;
  bool sessionLocked() const { return sessionLock && sessionLock->locked(); }
  void updateSessionLockOutputs();
  void sessionLockChanged(bool locked);
  wlr_fractional_scale_manager_v1 *fractionalScale = nullptr;
  wlr_screencopy_manager_v1 *screencopy = nullptr;
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  wlr_ext_foreign_toplevel_list_v1 *foreignToplevelList = nullptr;
  wlr_ext_image_copy_capture_manager_v1 *imageCopyCapture = nullptr;
  wlr_ext_foreign_toplevel_image_capture_source_manager_v1
      *foreignToplevelCaptureSource = nullptr;
#endif
#if LUDASH_WLR_HAS_DATA_CONTROL
  wlr_data_control_manager_v1 *dataControl = nullptr;
#endif
#if LUDASH_WLR_HAS_DRM_SYNCOBJ
  wlr_linux_drm_syncobj_manager_v1 *explicitSync = nullptr;
#endif
  wlr_xdg_output_manager_v1 *xdgOutput = nullptr;
  wlr_idle_inhibit_manager_v1 *idleInhibit = nullptr;
  wlr_input_method_manager_v2 *inputMethodManager = nullptr;
  wlr_text_input_manager_v3 *textInputManager = nullptr;
  wlr_virtual_keyboard_manager_v1 *virtualKeyboardManager = nullptr;

  // Registration order; stopped in reverse. See Register.cpp.
  std::vector<std::unique_ptr<Component>> components;
  std::vector<OutputState *> outputs;
  std::vector<XWaylandState *> xwaylandSurfaces;
  std::vector<LayerState *> layers;
  std::vector<XdgPopupState *> xdgPopups;
  std::vector<KeyboardState *> keyboards;
  // The physical keyboard that produced the latest key or modifier change.
  // Power buttons, mice and webcams also register as keyboards, so the first
  // device is rarely the one being typed on.
  wlr_keyboard *activePhysicalKeyboard = nullptr;
  std::vector<TextInputState *> textInputs;
  std::vector<PopupState *> inputPopups;
  std::vector<std::unique_ptr<DeferredClientRemoval>> deferredClientRemovals;
  std::vector<std::unique_ptr<ScheduledCallback>> scheduledCallbacks;
  InputMethodState *inputMethod = nullptr;
  TextInputState *activeTextInput = nullptr;
  int pointerDevices = 0;
  int pointerWindow = 0;
  int pointerTarget = 0;
  int pointerEdge = 0;
  bool pointerResize = false;
  bool pointerClientGrab = false;
  uint32_t pointerResizeEdges = 0;
  uint32_t pointerButton = 0;
  bool orbitPointerConsumed = false;
  // Implicit grab: while a button is held, the pressed surface keeps the
  // pointer, so its release always reaches it. Compared, never dereferenced:
  // wlroots clears pointer_state.focused_surface when the surface goes away.
  wlr_surface *pressedSurface = nullptr;
  PointF pressedSurfaceOrigin;
  PointF pointerLast;
  // Where the drag began, and the window then: stacking windows follow the
  // pointer's whole travel from there, so snapping never drifts.
  PointF pointerOrigin;
  Rect pointerStartGeometry;
  // A maximized or quick-tiled stacking window being moved goes back to its
  // own size once the pointer has moved.
  bool pointerRestore = false;
  // Where releasing a dragged stacking window would quick-tile it (xdg edges;
  // top alone maximizes), shown by an outline under the window.
  uint32_t pointerQuickTile = 0;
  wlr_scene_rect *quickTileOutline = nullptr;
  // A tiled window being dragged follows the pointer itself, outside its slot.
  bool pointerFollowing = false;
  // While arrange() lays windows out for a new window style.
  bool styleSwitch = false;
  // Stacking windows' outlines (WindowBorders.cpp): premultiplied colours for
  // the focused window and the others (applyVisualPreferences).
  float borderFocused[4]{};
  float borderUnfocused[4]{};
  void updateWindowBorder(ClientWindow *client);
  void applyDecorationMode(ClientWindow *client);
  std::chrono::steady_clock::time_point pointerPublishTime{};
  bool pointerPublishTimeValid = false;
  bool runtimeReleased = false;
  bool beginWindowPointer(uint32_t button);
  bool beginClientWindowPointer(ClientWindow *client, uint32_t serial,
                                uint32_t edges, bool validateSerial = true);
  // Pressing just outside a stacking window's edge resizes it from there.
  bool beginBorderResize(uint32_t button);
  // The edges of the window whose resize border is at (x, y), 0 for none.
  uint32_t resizeEdgesAt(double x, double y, ClientWindow **client) const;
  void startWindowPointer(ClientWindow *client, uint32_t button, uint32_t edges,
                          bool clientGrab);
  void showQuickTile(uint32_t edges, const Rect &outline, ClientWindow *client);
  bool updateWindowPointer();
  void finishWindowPointer(bool apply);

  explicit Impl(WaylandCompositor *owner);

  ~Impl();

  // Register.cpp: the runtime, every component, and their teardown.
  bool initialize();
  void shutdown();
  // Tears down clients, the scene and wlroots objects but keeps the Wayland
  // display and its event loop, so owners of event sources can remove them
  // before shutdown() destroys the loop.
  void releaseRuntime();
  template <typename ComponentType> bool registerComponent();

  // Runtime.cpp: renderer, scene, seat and the globals no component owns.
  bool createRuntime();
  // Opens the client sockets and starts delivering outputs and input devices.
  bool startRuntime();
  void destroyRuntime();

  // EventLoop.cpp
  bool fail(const char *message);
  int runEventLoop(bool profile);
  void terminateEventLoop(int exitCode);
  bool scheduleCallback(int delayMilliseconds, std::function<void()> callback);
  void cancelScheduledCallbacks();

  Size outputSize() const;
  // Background, layer-shell work area and window layout after a size change.
  void relayoutOutputs();
  void loadOutputSizePreferences();

  Json displaySnapshot() const;
  Json displayModes() const;
  bool configureDisplay(const Json &changes, std::string *error);
  void confirmDisplay();
  void revertDisplay();
  void clearPendingDisplay();

  int mappedLayerCount() const;

  bool inputBridgeReady() const;

  void updateBackground();
  void updateWindowGlass();
  void showSplash();
  // The desktop is up (or will not come): the splash fades out.
  void finishSplash();
  void advanceSplash(std::uint64_t nowNs);
  void updateWindowCorners();
  void updateNightLight();

  void arrangeLayers();
  void restoreLayerFocus();
  // A shell overlay (a layer surface taking the keyboard) is up.
  bool shellOverlayHasKeyboard() const;
  // A window mapping now: it is a dialog for the shell's overlay when one is up.
  void adoptShellDialog(ClientWindow *client);
  // No overlay is left: shell dialogs become ordinary windows.
  void releaseShellDialogs();
  static SceneAnimationRectangle layerRectangle(const LayerState *state);
  bool animatesLayer(const LayerState *state) const;
  void addXdgPopup(wlr_xdg_popup *popup);
  void addXWaylandSurface(wlr_xwayland_surface *surface);
  void configureXdgPopup(XdgPopupState *state);
  wlr_scene_tree *sceneForSurface(wlr_surface *surface) const;
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  wlr_scene_tree *captureSceneForSurface(wlr_surface *surface) const;
#endif

  bool resizePrimaryOutput(const std::string &preset, std::string *error);

  ClientWindow *clientForSurface(wlr_surface *surface) const;

  wlr_surface *surfaceAt(double lx, double ly, double *sx, double *sy) const;

  void processPointerMotion(uint32_t time);

  void rebasePointer();
  void setCursorImage(const char *name);
  void setClientCursor(wlr_surface *surface, int32_t hotspotX, int32_t hotspotY);
  std::string cursorImage = "default";
  std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();
  // Rebases once, on the next event loop iteration.
  void schedulePointerRebase();
  bool pointerRebaseScheduled = false;

  wlr_keyboard *preferredKeyboard() const;

  void restorePreferredKeyboard();

  void focusSurface(wlr_surface *surface);

  void updateSeatCapabilities();

  void addKeyboard(wlr_keyboard *keyboard, bool isVirtual,
                   wl_client *ownerClient = nullptr);

  bool inputMethodVirtualKeyboard(const KeyboardState *state) const;

  void applyKeyboardConfig();

  TextInputState *textState(wlr_text_input_v3 *text) const;

  void syncTextInputToMethod(TextInputState *state, bool activate);

  void deactivateTextInput(TextInputState *state);

  void updateTextInputFocus(wlr_surface *surface);

  void positionInputPopup(PopupState *popup);

  void configureInitialToplevel(ClientWindow *client);

  void addXdgToplevel(wlr_xdg_surface *surface, wlr_xdg_toplevel *toplevel);

  void deferClientRemoval(ClientWindow *client);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  void updateClientLegacyForeignToplevel(ClientWindow *client);
#endif
};

} // namespace LunaDash
