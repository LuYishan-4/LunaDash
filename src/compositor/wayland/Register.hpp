#pragma once

// Private wlroots runtime state. Public callers use WaylandCompositor.hpp.
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "core/templates/WaylandSlot.hpp"
#include <QJsonArray>
#include <QList>
#include <QPointF>
#include <QSize>
class QSocketNotifier;
class QTimer;

namespace LunaDash {
using Templates::WaylandSlot;
class WaylandCompositor::Impl {
public:
  template <typename Owner> using Slot = WaylandSlot<Owner>;

  struct OutputState {
    Impl *impl = nullptr;
    wlr_output *output = nullptr;
    wlr_scene_output *sceneOutput = nullptr;
    // Keep a short frame tail after a screencopy request is consumed so
    // portal clients have time to queue their next streaming frame.
    int screencopyKeepalive = 0;
    Slot<OutputState> frame;
    Slot<OutputState> destroy;
    Slot<OutputState> requestState;
  };

  struct ToplevelState {
    Impl *impl = nullptr;
    ClientWindow *client = nullptr;
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
    wlr_scene *imageCaptureScene = nullptr;
    wlr_scene_tree *imageCaptureTree = nullptr;
    wlr_ext_foreign_toplevel_handle_v1 *foreignHandle = nullptr;
    wlr_ext_image_capture_source_v1 *imageCaptureSource = nullptr;
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
    // Compatibility global used by native Wayland utilities such as
    // GPU Screen Recorder UI to discover the active/focused toplevel.
    wlr_foreign_toplevel_handle_v1 *legacyForeignHandle = nullptr;
#endif
    Slot<ToplevelState> map;
    Slot<ToplevelState> unmap;
    Slot<ToplevelState> commit;
    Slot<ToplevelState> destroy;
    Slot<ToplevelState> setTitle;
    Slot<ToplevelState> setAppId;
    Slot<ToplevelState> setParent;
    Slot<ToplevelState> requestMinimize;
    Slot<ToplevelState> requestMaximize;
    Slot<ToplevelState> requestFullscreen;
    Slot<ToplevelState> requestMove;
    Slot<ToplevelState> requestResize;
  };

  struct XWaylandState {
    Impl *impl = nullptr;
    ClientWindow *client = nullptr;
    wlr_xwayland_surface *surface = nullptr;
    Slot<XWaylandState> map;
    Slot<XWaylandState> unmap;
    Slot<XWaylandState> destroy;
    Slot<XWaylandState> requestConfigure;
    Slot<XWaylandState> requestMove;
    Slot<XWaylandState> requestResize;
    Slot<XWaylandState> requestMinimize;
    Slot<XWaylandState> requestMaximize;
    Slot<XWaylandState> requestFullscreen;
    Slot<XWaylandState> requestActivate;
    Slot<XWaylandState> setTitle;
    Slot<XWaylandState> setClass;
    Slot<XWaylandState> setParent;
    Slot<XWaylandState> setPid;
    Slot<XWaylandState> setGeometry;
    Slot<XWaylandState> setOverrideRedirect;
  };

  struct LayerState {
    Impl *impl = nullptr;
    wlr_layer_surface_v1 *surface = nullptr;
    wlr_scene_layer_surface_v1 *sceneLayer = nullptr;
    bool mapped = false;
    bool configured = false;
    Slot<LayerState> map;
    Slot<LayerState> unmap;
    Slot<LayerState> commit;
    Slot<LayerState> destroy;
  };

  struct XdgPopupState {
    Impl *impl = nullptr;
    wlr_xdg_popup *popup = nullptr;
    wlr_scene_tree *sceneTree = nullptr;
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
    wlr_scene_tree *captureTree = nullptr;
#endif
    Slot<XdgPopupState> commit;
    Slot<XdgPopupState> reposition;
    Slot<XdgPopupState> destroy;
  };

  struct KeyboardState {
    Impl *impl = nullptr;
    wlr_keyboard *keyboard = nullptr;
    bool virtualKeyboard = false;
    QSet<uint32_t> consumedKeys;
    bool metaTapPending = false;
    int metaTapKeycode = -1;
    Slot<KeyboardState> key;
    Slot<KeyboardState> modifiers;
    Slot<KeyboardState> destroy;
  };

  struct InputMethodState {
    Impl *impl = nullptr;
    wlr_input_method_v2 *method = nullptr;
    Slot<InputMethodState> commit;
    Slot<InputMethodState> newPopup;
    Slot<InputMethodState> grabKeyboard;
    Slot<InputMethodState> destroy;
  };

  struct TextInputState {
    Impl *impl = nullptr;
    wlr_text_input_v3 *text = nullptr;
    Slot<TextInputState> enable;
    Slot<TextInputState> commit;
    Slot<TextInputState> disable;
    Slot<TextInputState> destroy;
  };

  struct PopupState {
    Impl *impl = nullptr;
    wlr_input_popup_surface_v2 *popup = nullptr;
    wlr_scene_tree *sceneTree = nullptr;
    Slot<PopupState> destroy;
  };

  WaylandCompositor *q = nullptr;
  QByteArray socketName;
  bool fullscreen = false;
  bool nested = false;
  QString rendererPreference;

  wl_display *display = nullptr;
  wl_event_loop *eventLoop = nullptr;
  wlr_backend *backend = nullptr;
  wlr_compositor *compositor = nullptr;
  wlr_renderer *renderer = nullptr;
  wlr_allocator *allocator = nullptr;
  wlr_output_layout *outputLayout = nullptr;
  wlr_scene *scene = nullptr;
  wlr_scene_output_layout *sceneLayout = nullptr;
  wlr_scene_tree *backgroundLayer = nullptr;
  wlr_scene_tree *bottomLayer = nullptr;
  wlr_scene_tree *normalLayer = nullptr;
  wlr_scene_tree *animationLayer = nullptr;
  wlr_scene_tree *topLayer = nullptr;
  wlr_scene_tree *fullscreenLayer = nullptr;
  wlr_scene_tree *overlayLayer = nullptr;
  wlr_scene_rect *background = nullptr;
  wlr_output *primaryOutput = nullptr;
  wlr_output *pendingDisplay = nullptr;
  wlr_output_state previousDisplay{};
  QTimer *displayRevertTimer = nullptr;
  QString displayError;
  QRect usableArea{0, 0, 1440, 900};

  wlr_xdg_shell *xdgShell = nullptr;
  wlr_layer_shell_v1 *layerShell = nullptr;
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  wlr_foreign_toplevel_manager_v1 *foreignToplevelManager = nullptr;
#endif
  wlr_seat *seat = nullptr;
  wlr_cursor *cursor = nullptr;
  wlr_xcursor_manager *cursorManager = nullptr;
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

  QSocketNotifier *waylandNotifier = nullptr;
  QList<OutputState *> outputs;
  QList<XWaylandState *> xwaylandSurfaces;
  QList<LayerState *> layers;
  QList<XdgPopupState *> xdgPopups;
  QList<KeyboardState *> keyboards;
  QList<TextInputState *> textInputs;
  QList<PopupState *> inputPopups;
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
  QPointF pointerLast;
  bool beginWindowPointer(uint32_t button);
  bool beginClientWindowPointer(ClientWindow *client, uint32_t serial,
                                uint32_t edges);
  bool updateWindowPointer();
  void finishWindowPointer(bool apply);

  Slot<Impl> newOutput;
  Slot<Impl> newInput;
  Slot<Impl> newXdgSurface;
  Slot<Impl> newXdgPopup;
  Slot<Impl> newLayerSurface;
  Slot<Impl> requestCursor;
  Slot<Impl> requestSelection;
  Slot<Impl> keyboardFocusChange;
  Slot<Impl> cursorMotion;
  Slot<Impl> cursorMotionAbsolute;
  Slot<Impl> cursorButton;
  Slot<Impl> cursorAxis;
  Slot<Impl> cursorFrame;
  Slot<Impl> newInputMethod;
  Slot<Impl> newTextInput;
  Slot<Impl> newVirtualKeyboard;
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  Slot<Impl> foreignToplevelCaptureRequest;
#endif

  explicit Impl(WaylandCompositor *owner, const QByteArray &socket,
                bool wantsFullscreen, const QString &renderer);

  ~Impl();

  bool initialize();

  bool fail(const char *message);

  void dispatch();

  void shutdown();

  QSize outputSize() const;

  QJsonObject displaySnapshot() const;
  QJsonArray displayModes() const;
  bool configureDisplay(const QJsonObject &changes, QString *error);
  void confirmDisplay();
  void revertDisplay();
  void clearPendingDisplay();

  int mappedLayerCount() const;

  bool inputBridgeReady() const;

  void updateBackground();

  void arrangeLayers();
  void restoreLayerFocus();
  void addXdgPopup(wlr_xdg_popup *popup);
  void addXWaylandSurface(wlr_xwayland_surface *surface);
  void configureXdgPopup(XdgPopupState *state);
  wlr_scene_tree *sceneForSurface(wlr_surface *surface) const;
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  wlr_scene_tree *captureSceneForSurface(wlr_surface *surface) const;
#endif
  static void handleNewXdgPopup(wl_listener *listener, void *data);
  static void handleXdgPopupCommit(wl_listener *listener, void *data);
  static void handleXdgPopupReposition(wl_listener *listener, void *data);
  static void handleXdgPopupDestroy(wl_listener *listener, void *data);

  bool resizePrimaryOutput(const QString &preset, QString *error);

  ClientWindow *clientForSurface(wlr_surface *surface) const;

  wlr_surface *surfaceAt(double lx, double ly, double *sx, double *sy) const;

  void processPointerMotion(uint32_t time);

  wlr_keyboard *preferredKeyboard() const;

  void restorePreferredKeyboard();

  void focusSurface(wlr_surface *surface);

  void updateSeatCapabilities();

  void addKeyboard(wlr_keyboard *keyboard, bool isVirtual);

  void applyKeyboardConfig();

  TextInputState *textState(wlr_text_input_v3 *text) const;

  void syncTextInputToMethod(TextInputState *state, bool activate);

  void deactivateTextInput(TextInputState *state);

  void updateTextInputFocus(wlr_surface *surface);

  void positionInputPopup(PopupState *popup);

  static void handleNewOutput(wl_listener *listener, void *data);

  static void handleOutputFrame(wl_listener *listener, void *);

  static void handleOutputRequestState(wl_listener *listener, void *data);

  static void handleOutputDestroy(wl_listener *listener, void *);

  static void handleNewInput(wl_listener *listener, void *data);

  static void handleKeyboardKey(wl_listener *listener, void *data);

  static void handleKeyboardModifiers(wl_listener *listener, void *);

  static void handleKeyboardDestroy(wl_listener *listener, void *);

  static void handleCursorMotion(wl_listener *listener, void *data);

  static void handleCursorMotionAbsolute(wl_listener *listener, void *data);

  static void handleCursorButton(wl_listener *listener, void *data);

  static void handleCursorAxis(wl_listener *listener, void *data);

  static void handleCursorFrame(wl_listener *listener, void *);

  static void handleRequestCursor(wl_listener *listener, void *data);

  static void handleRequestSelection(wl_listener *listener, void *data);

  static void handleKeyboardFocusChange(wl_listener *listener, void *data);

  void configureInitialToplevel(ClientWindow *client);

  void addXdgToplevel(wlr_xdg_surface *surface, wlr_xdg_toplevel *toplevel);

#if WLR_VERSION_MINOR < 18
  static void handleNewXdgSurface(wl_listener *listener, void *data);
#else
  static void handleNewXdgToplevel(wl_listener *listener, void *data);
#endif

  static void handleToplevelMap(wl_listener *listener, void *);

  static void handleToplevelUnmap(wl_listener *listener, void *);

  static void handleToplevelCommit(wl_listener *listener, void *);

  static void handleToplevelMetadata(wl_listener *listener, void *);

  static void handleToplevelParent(wl_listener *listener, void *);

  static void handleToplevelMinimize(wl_listener *listener, void *);

  static void handleToplevelMaximize(wl_listener *listener, void *);

  static void handleToplevelFullscreen(wl_listener *listener, void *);

  static void handleToplevelMove(wl_listener *listener, void *data);
  static void handleToplevelResize(wl_listener *listener, void *data);

  static void handleToplevelDestroy(wl_listener *listener, void *);

  static void handleXWaylandMap(wl_listener *listener, void *data);
  static void handleXWaylandUnmap(wl_listener *listener, void *data);
  static void handleXWaylandDestroy(wl_listener *listener, void *data);
  static void handleXWaylandConfigure(wl_listener *listener, void *data);
  static void handleXWaylandMove(wl_listener *listener, void *data);
  static void handleXWaylandResize(wl_listener *listener, void *data);
  static void handleXWaylandMinimize(wl_listener *listener, void *data);
  static void handleXWaylandMaximize(wl_listener *listener, void *data);
  static void handleXWaylandFullscreen(wl_listener *listener, void *data);
  static void handleXWaylandActivate(wl_listener *listener, void *data);
  static void handleXWaylandMetadata(wl_listener *listener, void *data);

  static void handleNewLayerSurface(wl_listener *listener, void *data);

  static void handleLayerMap(wl_listener *listener, void *);

  static void handleLayerUnmap(wl_listener *listener, void *);

  static void handleLayerCommit(wl_listener *listener, void *);

  static void handleLayerDestroy(wl_listener *listener, void *);

  static void handleNewInputMethod(wl_listener *listener, void *data);

  static void handleInputMethodCommit(wl_listener *listener, void *);

  static void handleInputMethodPopup(wl_listener *listener, void *data);

  static void handleInputPopupDestroy(wl_listener *listener, void *);

  static void handleInputMethodGrab(wl_listener *listener, void *data);

  static void handleInputMethodDestroy(wl_listener *listener, void *);

  static void handleNewTextInput(wl_listener *listener, void *data);

  static void handleTextInputEnable(wl_listener *listener, void *);

  static void handleTextInputCommit(wl_listener *listener, void *);

  static void handleTextInputDisable(wl_listener *listener, void *);

  static void handleTextInputDestroy(wl_listener *listener, void *);

  static void handleNewVirtualKeyboard(wl_listener *listener, void *data);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  static void updateForeignToplevel(ToplevelState *state);
  static void createForeignToplevel(ToplevelState *state);
  static void destroyForeignToplevel(ToplevelState *state);
  static void handleForeignToplevelCaptureRequest(wl_listener *listener,
                                                  void *data);
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  static void updateLegacyForeignToplevel(ToplevelState *state);
  static void createLegacyForeignToplevel(ToplevelState *state);
  static void destroyLegacyForeignToplevel(ToplevelState *state);
#endif
};

} // namespace LunaDash
