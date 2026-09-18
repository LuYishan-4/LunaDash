#include "compositor/WaylandCompositor/WaylandCompositor.hpp"

#include "compositor/ClientWindow/ClientWindow.hpp"
#include "compositor/wlroots/ClientLaunchCompat.hpp"
#include "core/templates/WaylandListener.hpp"
#include "compositor/SessionActions/SessionActions.hpp"
#include "compositor/SessionEnvironment/SessionEnvironment.hpp"
#include "compositor/ShellModules/ShellModules.hpp"
#include "compositor/SystemStatus/SystemStatus.hpp"
#include "compositor/UpdateChecker/UpdateChecker.hpp"
#include "compositor/WindowRules/WindowRules.hpp"
#include "compositor/ipc/ControlServer/ControlServer.hpp"
#include "compositor/plugins/PluginManager/PluginManager.hpp"
#include "compositor/wlroots/WlrootsCompat.hpp"
#include "compositor/wlroots/WlrootsHeaders.hpp"
#include "compositor/xwayland/XWaylandSupport/XWaylandSupport.hpp"
#include "config/DesktopPreferences/DesktopPreferences.hpp"
#include "config/Localization/Localization.hpp"
#include "desktop/AudioSettings/AudioSettings.hpp"
#include "desktop/DefaultApplications/DefaultApplications.hpp"
#include "desktop/InputSettings/InputSettings.hpp"
#include "desktop/NetworkStatus/NetworkStatus.hpp"
#include "desktop/PowerSettings/PowerSettings.hpp"
#include "desktop/ShortcutSettings/ShortcutSettings.hpp"
#include "desktop/SystemTools/SystemTools.hpp"
#include "desktop/WallpaperSettings/WallpaperSettings.hpp"

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
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


namespace LuDash {
namespace {

using Templates::ListenerSlot;
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

QString safeUtf8(const char *text) {
  return QString::fromUtf8(text ? text : "");
}

bool isUtilityWindow(const QString &appId) {
  return appId == QLatin1String("io.github.bugaevc.wl-clipboard");
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

uint32_t shortcutModifiers(uint32_t wlrModifiers) {
  uint32_t result = 0;
  if (wlrModifiers & WLR_MODIFIER_LOGO)
    result |= ShortcutMeta;
  if (wlrModifiers & WLR_MODIFIER_CTRL)
    result |= ShortcutControl;
  if (wlrModifiers & WLR_MODIFIER_ALT)
    result |= ShortcutAlt;
  if (wlrModifiers & WLR_MODIFIER_SHIFT)
    result |= ShortcutShift;
  return result;
}

bool keypadSymbol(xkb_keysym_t symbol) {
  return symbol >= XKB_KEY_KP_Space && symbol <= XKB_KEY_KP_Equal;
}

} // namespace

class WaylandCompositor::Impl {
public:
  template <typename Owner> using Slot = ListenerSlot<Owner>;

  struct OutputState {
    Impl *impl = nullptr;
    wlr_output *output = nullptr;
    wlr_scene_output *sceneOutput = nullptr;
    Slot<OutputState> frame;
    Slot<OutputState> destroy;
    Slot<OutputState> requestState;
  };

  struct ToplevelState {
    Impl *impl = nullptr;
    ClientWindow *client = nullptr;
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
  };

  struct LayerState {
    Impl *impl = nullptr;
    wlr_layer_surface_v1 *surface = nullptr;
    wlr_scene_layer_surface_v1 *sceneLayer = nullptr;
    bool mapped = false;
    Slot<LayerState> map;
    Slot<LayerState> unmap;
    Slot<LayerState> commit;
    Slot<LayerState> destroy;
  };

  struct KeyboardState {
    Impl *impl = nullptr;
    wlr_keyboard *keyboard = nullptr;
    bool virtualKeyboard = false;
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
  wlr_renderer *renderer = nullptr;
  wlr_allocator *allocator = nullptr;
  wlr_output_layout *outputLayout = nullptr;
  wlr_scene *scene = nullptr;
  wlr_scene_output_layout *sceneLayout = nullptr;
  wlr_scene_tree *backgroundLayer = nullptr;
  wlr_scene_tree *bottomLayer = nullptr;
  wlr_scene_tree *normalLayer = nullptr;
  wlr_scene_tree *topLayer = nullptr;
  wlr_scene_tree *overlayLayer = nullptr;
  wlr_scene_rect *background = nullptr;
  wlr_output *primaryOutput = nullptr;
  QRect usableArea{0, 0, 1440, 900};

  wlr_xdg_shell *xdgShell = nullptr;
  wlr_layer_shell_v1 *layerShell = nullptr;
  wlr_seat *seat = nullptr;
  wlr_cursor *cursor = nullptr;
  wlr_xcursor_manager *cursorManager = nullptr;
  wlr_screencopy_manager_v1 *screencopy = nullptr;
  wlr_xdg_output_manager_v1 *xdgOutput = nullptr;
  wlr_idle_inhibit_manager_v1 *idleInhibit = nullptr;
  wlr_input_method_manager_v2 *inputMethodManager = nullptr;
  wlr_text_input_manager_v3 *textInputManager = nullptr;
  wlr_virtual_keyboard_manager_v1 *virtualKeyboardManager = nullptr;

  QSocketNotifier *waylandNotifier = nullptr;
  QList<OutputState *> outputs;
  QList<LayerState *> layers;
  QList<KeyboardState *> keyboards;
  QList<TextInputState *> textInputs;
  QList<PopupState *> inputPopups;
  InputMethodState *inputMethod = nullptr;
  TextInputState *activeTextInput = nullptr;
  int pointerDevices = 0;

  Slot<Impl> newOutput;
  Slot<Impl> newInput;
  Slot<Impl> newXdgSurface;
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

  explicit Impl(WaylandCompositor *owner, const QByteArray &socket,
                bool wantsFullscreen, const QString &renderer)
      : q(owner), socketName(socket), fullscreen(wantsFullscreen),
        nested(!qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty() ||
               !qEnvironmentVariable("DISPLAY").isEmpty()),
        rendererPreference(renderer) {}

  ~Impl() { shutdown(); }

  bool initialize() {
    if (rendererPreference == "opengl" || rendererPreference == "gles") {
      if (qEnvironmentVariableIsEmpty("WLR_RENDERER"))
        qputenv("WLR_RENDERER", "gles2");
    }

    wlr_log_init(WLR_INFO, nullptr);
    display = wl_display_create();
    if (!display)
      return fail("Could not create the Wayland display.");
    eventLoop = wl_display_get_event_loop(display);

    backend = WlrootsCompat::createBackend(display);
    if (!backend)
      return fail("wlroots could not create a backend.");
    renderer = wlr_renderer_autocreate(backend);
    if (!renderer)
      return fail("wlroots could not create a renderer.");
    if (!wlr_renderer_init_wl_display(renderer, display))
      return fail("wlroots could not initialize renderer formats.");
    allocator = wlr_allocator_autocreate(backend, renderer);
    if (!allocator)
      return fail("wlroots could not create a buffer allocator.");

    wlr_compositor_create(display, 5, renderer);
    wlr_subcompositor_create(display);
    wlr_data_device_manager_create(display);
    wlr_viewporter_create(display);

    outputLayout = WlrootsCompat::createOutputLayout(display);
    if (!outputLayout)
      return fail("wlroots could not create the output layout.");

    scene = wlr_scene_create();
    if (!scene)
      return fail("wlroots could not create the scene graph.");
    sceneLayout = wlr_scene_attach_output_layout(scene, outputLayout);
    backgroundLayer = wlr_scene_tree_create(&scene->tree);
    bottomLayer = wlr_scene_tree_create(&scene->tree);
    normalLayer = wlr_scene_tree_create(&scene->tree);
    topLayer = wlr_scene_tree_create(&scene->tree);
    overlayLayer = wlr_scene_tree_create(&scene->tree);
    const float color[4] = {0.07f, 0.09f, 0.18f, 1.0f};
    background = wlr_scene_rect_create(backgroundLayer, 1440, 900, color);

    xdgShell = wlr_xdg_shell_create(display, 3);
    layerShell = wlr_layer_shell_v1_create(display, 4);
    screencopy = wlr_screencopy_manager_v1_create(display);
    xdgOutput = wlr_xdg_output_manager_v1_create(display, outputLayout);
    idleInhibit = wlr_idle_inhibit_v1_create(display);
    inputMethodManager = wlr_input_method_manager_v2_create(display);
    textInputManager = wlr_text_input_manager_v3_create(display);
    virtualKeyboardManager = wlr_virtual_keyboard_manager_v1_create(display);
    if (!xdgShell || !layerShell || !screencopy || !xdgOutput ||
        !idleInhibit || !inputMethodManager || !textInputManager ||
        !virtualKeyboardManager)
      return fail("wlroots could not create required Wayland protocol globals.");

    seat = wlr_seat_create(display, "seat0");
    cursor = wlr_cursor_create();
    cursorManager = wlr_xcursor_manager_create(
        nullptr, qEnvironmentVariableIntValue("XCURSOR_SIZE") > 0
                     ? qEnvironmentVariableIntValue("XCURSOR_SIZE")
                     : 24);
    if (!seat || !cursor || !cursorManager)
      return fail("wlroots could not create the seat/cursor stack.");
    wlr_cursor_attach_output_layout(cursor, outputLayout);

    attachListener(&backend->events.new_output, newOutput, this,
                   handleNewOutput);
    attachListener(&backend->events.new_input, newInput, this, handleNewInput);
#if WLR_VERSION_MINOR < 20
    attachListener(&xdgShell->events.new_surface, newXdgSurface, this,
                   handleNewXdgSurface);
#else
    // wlroots 0.20 emits new_surface before an xdg role is assigned. Listen to
    // new_toplevel so the object is fully role-initialized before tracking it.
    attachListener(&xdgShell->events.new_toplevel, newXdgSurface, this,
                   handleNewXdgToplevel);
#endif
    attachListener(&layerShell->events.new_surface, newLayerSurface, this,
                   handleNewLayerSurface);
    attachListener(&seat->events.request_set_cursor, requestCursor, this,
                   handleRequestCursor);
    attachListener(&seat->events.request_set_selection, requestSelection, this,
                   handleRequestSelection);
    attachListener(&seat->keyboard_state.events.focus_change,
                   keyboardFocusChange, this, handleKeyboardFocusChange);
    attachListener(&cursor->events.motion, cursorMotion, this,
                   handleCursorMotion);
    attachListener(&cursor->events.motion_absolute, cursorMotionAbsolute, this,
                   handleCursorMotionAbsolute);
    attachListener(&cursor->events.button, cursorButton, this,
                   handleCursorButton);
    attachListener(&cursor->events.axis, cursorAxis, this, handleCursorAxis);
    attachListener(&cursor->events.frame, cursorFrame, this,
                   handleCursorFrame);
    attachListener(WlrootsCompat::newInputMethodSignal(inputMethodManager),
                   newInputMethod, this, handleNewInputMethod);
    attachListener(WlrootsCompat::newTextInputSignal(textInputManager),
                   newTextInput, this, handleNewTextInput);
    attachListener(&virtualKeyboardManager->events.new_virtual_keyboard,
                   newVirtualKeyboard, this, handleNewVirtualKeyboard);

    wlr_seat_set_capabilities(seat,
                              WL_SEAT_CAPABILITY_POINTER |
                                  WL_SEAT_CAPABILITY_KEYBOARD);

    if (wl_display_add_socket(display, socketName.constData()) != 0)
      return fail("Could not create the requested Wayland socket.");

    const int fd = wl_event_loop_get_fd(eventLoop);
    waylandNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, q);
    QObject::connect(waylandNotifier, &QSocketNotifier::activated, q,
                     [this] { dispatch(); });
    if (auto *dispatcher = QAbstractEventDispatcher::instance()) {
      QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, q,
                       [this] { dispatch(); });
    }

    if (!wlr_backend_start(backend))
      return fail("wlroots could not start the backend.");

    updateBackground();
    qInfo().noquote() << "LunaDash wlroots socket:" << socketName
                      << "seat protocol >=" << WlrootsCompat::expectedSeatProtocolVersion();
    return true;
  }

  bool fail(const char *message) {
    qCritical().noquote() << "LunaDash wlroots:" << message;
    q->processFailure_ = true;
    QTimer::singleShot(0, q, [] { QCoreApplication::exit(2); });
    return false;
  }

  void dispatch() {
    if (!eventLoop)
      return;
    if (wl_event_loop_dispatch(eventLoop, 0) < 0) {
      q->processFailure_ = true;
      qCritical("LunaDash wlroots event dispatch failed.");
      QCoreApplication::exit(2);
      return;
    }
    if (display)
      wl_display_flush_clients(display);
  }

  void shutdown() {
    if (!display)
      return;
    if (waylandNotifier) {
      waylandNotifier->setEnabled(false);
      waylandNotifier->deleteLater();
      waylandNotifier = nullptr;
    }

    wl_display_destroy_clients(display);

    detachListener(newOutput);
    detachListener(newInput);
    detachListener(newXdgSurface);
    detachListener(newLayerSurface);
    detachListener(requestCursor);
    detachListener(requestSelection);
    detachListener(keyboardFocusChange);
    detachListener(cursorMotion);
    detachListener(cursorMotionAbsolute);
    detachListener(cursorButton);
    detachListener(cursorAxis);
    detachListener(cursorFrame);
    detachListener(newInputMethod);
    detachListener(newTextInput);
    detachListener(newVirtualKeyboard);

    // Runtime wrappers keep listeners on wlroots-owned objects. Disconnect
    // every wrapper before destroying the scene/backend so late output/input
    // destruction cannot call back into already-freed scene nodes.
    const auto outputStates = outputs;
    outputs.clear();
    primaryOutput = nullptr;
    for (auto *state : outputStates) {
      if (!state)
        continue;
      detachListener(state->frame);
      detachListener(state->destroy);
      detachListener(state->requestState);
      delete state;
    }

    const auto layerStates = layers;
    layers.clear();
    for (auto *state : layerStates) {
      if (!state)
        continue;
      detachListener(state->map);
      detachListener(state->unmap);
      detachListener(state->commit);
      detachListener(state->destroy);
      delete state;
    }

    const auto keyboardStates = keyboards;
    keyboards.clear();
    for (auto *state : keyboardStates) {
      if (!state)
        continue;
      detachListener(state->key);
      detachListener(state->modifiers);
      detachListener(state->destroy);
      delete state;
    }

    const auto textInputStates = textInputs;
    textInputs.clear();
    activeTextInput = nullptr;
    for (auto *state : textInputStates) {
      if (!state)
        continue;
      detachListener(state->enable);
      detachListener(state->commit);
      detachListener(state->disable);
      detachListener(state->destroy);
      delete state;
    }

    const auto popupStates = inputPopups;
    inputPopups.clear();
    for (auto *state : popupStates) {
      if (!state)
        continue;
      detachListener(state->destroy);
      delete state;
    }

    if (inputMethod) {
      detachListener(inputMethod->commit);
      detachListener(inputMethod->newPopup);
      detachListener(inputMethod->grabKeyboard);
      detachListener(inputMethod->destroy);
      delete inputMethod;
      inputMethod = nullptr;
    }

    if (scene) {
      wlr_scene_node_destroy(&scene->tree.node);
      scene = nullptr;
    }
    sceneLayout = nullptr;
    backgroundLayer = nullptr;
    bottomLayer = nullptr;
    normalLayer = nullptr;
    topLayer = nullptr;
    overlayLayer = nullptr;
    background = nullptr;

    if (cursorManager) {
      wlr_xcursor_manager_destroy(cursorManager);
      cursorManager = nullptr;
    }
    WlrootsCompat::destroyRuntimeObjects(cursor, allocator, renderer, backend);
    cursor = nullptr;
    allocator = nullptr;
    renderer = nullptr;
    backend = nullptr;
    if (outputLayout) {
      wlr_output_layout_destroy(outputLayout);
      outputLayout = nullptr;
    }
    wl_display_destroy(display);
    display = nullptr;
    eventLoop = nullptr;
  }

  QSize outputSize() const {
    if (!primaryOutput)
      return {1440, 900};
    int width = 0;
    int height = 0;
    wlr_output_effective_resolution(primaryOutput, &width, &height);
    return {std::max(1, width), std::max(1, height)};
  }

  QJsonObject displaySnapshot() const {
    const QSize size = outputSize();
    return {{"width", size.width()},
            {"height", size.height()},
            {"scale", primaryOutput ? primaryOutput->scale : 1.0},
            {"refreshRate",
             primaryOutput ? static_cast<double>(primaryOutput->refresh) / 1000.0
                           : 0.0},
            {"output", primaryOutput ? safeUtf8(primaryOutput->name) : QString()},
            {"nested", nested},
            {"fullscreen", fullscreen}};
  }

  int mappedLayerCount() const {
    int count = 0;
    for (const auto *layer : layers)
      if (layer && layer->mapped)
        ++count;
    return count;
  }

  bool inputBridgeReady() const {
    return inputMethodManager && textInputManager && virtualKeyboardManager;
  }

  void updateBackground() {
    if (!background)
      return;
    const QSize size = outputSize();
    wlr_scene_rect_set_size(background, size.width(), size.height());
    const int palette = QSettings().value("appearance/wallpaper", 0).toInt();
    if (palette == 1) {
      const float color[4] = {0.06f, 0.20f, 0.16f, 1.0f};
      wlr_scene_rect_set_color(background, color);
    } else {
      const float color[4] = {0.07f, 0.09f, 0.18f, 1.0f};
      wlr_scene_rect_set_color(background, color);
    }
  }

  void arrangeLayers() {
    const QSize size = outputSize();
    wlr_box full{0, 0, size.width(), size.height()};
    wlr_box usable = full;
    for (auto *layer : layers) {
      if (!layer || !layer->sceneLayer || !layer->surface)
        continue;
      // wlroots 0.20 emits layer_shell.new_surface before the client's first
      // commit. wlr_scene_layer_surface_v1_configure() is only valid after the
      // role has been initialized by that commit.
      if (!layer->surface->initialized)
        continue;
      if (!layer->surface->output && primaryOutput)
        layer->surface->output = primaryOutput;
      wlr_scene_layer_surface_v1_configure(layer->sceneLayer, &full, &usable);
    }
    usableArea = QRect(usable.x, usable.y, usable.width, usable.height);
  }

  bool resizePrimaryOutput(const QString &preset, QString *error) {
    static const QHash<QString, QSize> sizes{
        {"1280x720", {1280, 720}},
        {"1440x900", {1440, 900}},
        {"1920x1080", {1920, 1080}},
    };
    if (!nested || fullscreen || !sizes.contains(preset) || !primaryOutput) {
      if (error)
        *error =
            "Choose a supported size in a windowed nested wlroots session.";
      return false;
    }

    const QSize size = sizes.value(preset);
    wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_custom_mode(&state, size.width(), size.height(), 0);
    const bool ok = wlr_output_commit_state(primaryOutput, &state);
    wlr_output_state_finish(&state);
    if (!ok) {
      if (error)
        *error = "The wlroots backend rejected this nested output size.";
      return false;
    }
    updateBackground();
    arrangeLayers();
    q->arrange();
    return true;
  }

  ClientWindow *clientForSurface(wlr_surface *surface) const {
    if (!surface || !surface->resource)
      return nullptr;
    wl_client *owner = wl_resource_get_client(surface->resource);
    for (const auto &client : q->clients_) {
      if (!client->surface || !client->surface->resource)
        continue;
      if (wl_resource_get_client(client->surface->resource) == owner)
        return client.get();
    }
    return nullptr;
  }

  wlr_surface *surfaceAt(double lx, double ly, double *sx, double *sy) const {
    if (!scene)
      return nullptr;
    wlr_scene_node *node =
        wlr_scene_node_at(&scene->tree.node, lx, ly, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER)
      return nullptr;
    auto *buffer = wlr_scene_buffer_from_node(node);
    auto *sceneSurface = wlr_scene_surface_try_from_buffer(buffer);
    return sceneSurface ? sceneSurface->surface : nullptr;
  }

  void processPointerMotion(uint32_t time) {
    double sx = 0;
    double sy = 0;
    wlr_surface *surface = surfaceAt(cursor->x, cursor->y, &sx, &sy);
    if (surface) {
      wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
      wlr_seat_pointer_notify_motion(seat, time, sx, sy);
    } else {
      wlr_seat_pointer_notify_clear_focus(seat);
      wlr_cursor_set_xcursor(cursor, cursorManager, "default");
    }
  }

  wlr_keyboard *preferredKeyboard() const {
    for (auto *state : keyboards)
      if (state && state->keyboard && !state->virtualKeyboard)
        return state->keyboard;
    for (auto *state : keyboards)
      if (state && state->keyboard)
        return state->keyboard;
    return nullptr;
  }

  void restorePreferredKeyboard() {
    if (auto *keyboard = preferredKeyboard())
      wlr_seat_set_keyboard(seat, keyboard);
  }

  void focusSurface(wlr_surface *surface) {
    if (!surface)
      return;
    wlr_keyboard *keyboard = preferredKeyboard();
    if (!keyboard)
      return;
    wlr_seat_set_keyboard(seat, keyboard);
    wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes,
                                   keyboard->num_keycodes,
                                   &keyboard->modifiers);
  }

  void updateSeatCapabilities() {
    uint32_t caps = 0;
    if (pointerDevices > 0)
      caps |= WL_SEAT_CAPABILITY_POINTER;
    if (!keyboards.isEmpty())
      caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    if (!caps)
      caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(seat, caps);
  }

  void addKeyboard(wlr_keyboard *keyboard, bool isVirtual) {
    if (!keyboard)
      return;
    if (!isVirtual) {
      QString error;
      if (!applyKeyboardPreferences(keyboard, desktopPreferences(), &error))
        qWarning().noquote() << "LunaDash keyboard:" << error;
    }

    auto *state = new KeyboardState;
    state->impl = this;
    state->keyboard = keyboard;
    state->virtualKeyboard = isVirtual;
    attachListener(&keyboard->events.key, state->key, state,
                   handleKeyboardKey);
    attachListener(&keyboard->events.modifiers, state->modifiers, state,
                   handleKeyboardModifiers);
    attachListener(&keyboard->base.events.destroy, state->destroy, state,
                   handleKeyboardDestroy);
    keyboards.append(state);
    if (!isVirtual)
      wlr_seat_set_keyboard(seat, keyboard);
    else
      restorePreferredKeyboard();
    updateSeatCapabilities();
  }

  void applyKeyboardConfig() {
    for (auto *state : keyboards) {
      if (!state || state->virtualKeyboard)
        continue;
      QString error;
      if (!applyKeyboardPreferences(state->keyboard, desktopPreferences(),
                                    &error))
        qWarning().noquote() << "LunaDash keyboard:" << error;
    }
  }

  TextInputState *textState(wlr_text_input_v3 *text) const {
    for (auto *state : textInputs)
      if (state && state->text == text)
        return state;
    return nullptr;
  }

  void syncTextInputToMethod(TextInputState *state, bool activate) {
    if (!state || !state->text || !inputMethod || !inputMethod->method)
      return;
    auto *text = state->text;
    auto *method = inputMethod->method;

    if (activate) {
      activeTextInput = state;
      wlr_input_method_v2_send_activate(method);
    }

    const auto &current = text->current;
    if (current.features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT) {
      wlr_input_method_v2_send_surrounding_text(
          method, current.surrounding.text ? current.surrounding.text : "",
          current.surrounding.cursor, current.surrounding.anchor);
    }
    if (current.features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE) {
      wlr_input_method_v2_send_content_type(
          method, current.content_type.hint, current.content_type.purpose);
    }
    wlr_input_method_v2_send_text_change_cause(method,
                                                current.text_change_cause);
    wlr_input_method_v2_send_done(method);
  }

  void deactivateTextInput(TextInputState *state) {
    if (!state || activeTextInput != state)
      return;
    if (inputMethod && inputMethod->method) {
      wlr_input_method_v2_send_deactivate(inputMethod->method);
      wlr_input_method_v2_send_done(inputMethod->method);
    }
    activeTextInput = nullptr;
  }

  void updateTextInputFocus(wlr_surface *surface) {
    for (auto *state : textInputs) {
      if (!state || !state->text)
        continue;
      auto *text = state->text;
      if (text->focused_surface && text->focused_surface != surface) {
        deactivateTextInput(state);
        if (text->focused_surface->resource)
          wlr_text_input_v3_send_leave(text);
      }

      if (!surface || !surface->resource || !text->resource)
        continue;
      if (wl_resource_get_client(surface->resource) ==
          wl_resource_get_client(text->resource)) {
        if (text->focused_surface != surface)
          wlr_text_input_v3_send_enter(text, surface);
        if (text->current_enabled || text->pending_enabled)
          syncTextInputToMethod(state, true);
      }
    }
  }

  void positionInputPopup(PopupState *popup) {
    if (!popup || !popup->sceneTree || !activeTextInput ||
        !activeTextInput->text)
      return;
    wlr_surface *focus = activeTextInput->text->focused_surface;
    ClientWindow *client = clientForSurface(focus);
    if (!client)
      return;
    const auto &rect = activeTextInput->text->current.cursor_rectangle;
    wlr_scene_node_set_position(&popup->sceneTree->node,
                                client->geometry.x() + rect.x,
                                client->geometry.y() + rect.y + rect.height);
  }

  static void handleNewOutput(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *output = static_cast<wlr_output *>(data);
    if (!self || !output)
      return;

    if (!wlr_output_init_render(output, self->allocator, self->renderer)) {
      self->fail("Could not initialize output rendering.");
      return;
    }

    wlr_output_state pending;
    wlr_output_state_init(&pending);
    wlr_output_state_set_enabled(&pending, true);
    if (auto *mode = wlr_output_preferred_mode(output))
      wlr_output_state_set_mode(&pending, mode);
    else if (!self->fullscreen)
      wlr_output_state_set_custom_mode(&pending, 1440, 900, 60000);

    if (!wlr_output_commit_state(output, &pending))
      qWarning("wlroots rejected the preferred output state.");
    wlr_output_state_finish(&pending);

    auto *state = new OutputState;
    state->impl = self;
    state->output = output;
    auto *layoutOutput =
        wlr_output_layout_add_auto(self->outputLayout, output);
    state->sceneOutput = wlr_scene_output_create(self->scene, output);
    if (layoutOutput && state->sceneOutput)
      wlr_scene_output_layout_add_output(self->sceneLayout, layoutOutput,
                                         state->sceneOutput);

    attachListener(&output->events.frame, state->frame, state,
                   handleOutputFrame);
    attachListener(&output->events.destroy, state->destroy, state,
                   handleOutputDestroy);
    attachListener(&output->events.request_state, state->requestState, state,
                   handleOutputRequestState);
    self->outputs.append(state);

    if (!self->primaryOutput)
      self->primaryOutput = output;
    self->updateBackground();
    self->arrangeLayers();
    self->q->arrange();
    wlr_cursor_set_xcursor(self->cursor, self->cursorManager, "default");
  }

  static void handleOutputFrame(wl_listener *listener, void *) {
    auto *state = listenerOwner<OutputState>(listener);
    if (!state || !state->sceneOutput)
      return;
    if (!wlr_scene_output_commit(state->sceneOutput, nullptr))
      qWarning("wlroots scene output commit failed.");
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(state->sceneOutput, &now);
  }

  static void handleOutputRequestState(wl_listener *listener, void *data) {
    auto *state = listenerOwner<OutputState>(listener);
    auto *event = static_cast<wlr_output_event_request_state *>(data);
    if (state && event)
      wlr_output_commit_state(state->output, event->state);
  }

  static void handleOutputDestroy(wl_listener *listener, void *) {
    auto *state = listenerOwner<OutputState>(listener);
    if (!state)
      return;
    auto *self = state->impl;
    detachListener(state->frame);
    detachListener(state->destroy);
    detachListener(state->requestState);
    if (self->primaryOutput == state->output)
      self->primaryOutput = nullptr;
    self->outputs.removeAll(state);
    for (auto *candidate : self->outputs)
      if (candidate && candidate->output) {
        self->primaryOutput = candidate->output;
        break;
      }
    delete state;
    self->updateBackground();
    self->arrangeLayers();
    self->q->arrange();
  }

  static void handleNewInput(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *device = static_cast<wlr_input_device *>(data);
    if (!self || !device)
      return;
    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
      self->addKeyboard(wlr_keyboard_from_input_device(device), false);
      break;
    case WLR_INPUT_DEVICE_POINTER:
      ++self->pointerDevices;
      wlr_cursor_attach_input_device(self->cursor, device);
      self->updateSeatCapabilities();
      break;
    default:
      break;
    }
  }

  static void handleKeyboardKey(wl_listener *listener, void *data) {
    auto *state = listenerOwner<KeyboardState>(listener);
    auto *event = static_cast<wlr_keyboard_key_event *>(data);
    if (!state || !event || !state->keyboard)
      return;
    auto *self = state->impl;
    auto *keyboard = state->keyboard;
    wlr_seat_set_keyboard(self->seat, keyboard);

    const uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *symbols = nullptr;
    const int count =
        keyboard->xkb_state
            ? xkb_state_key_get_syms(keyboard->xkb_state, keycode, &symbols)
            : 0;

    bool handled = false;
    if (!state->virtualKeyboard &&
        event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
        !self->q->shortcutCapture_) {
      const uint32_t modifiers =
          shortcutModifiers(wlr_keyboard_get_modifiers(keyboard));
      for (int i = 0; i < count && !handled; ++i) {
        const QString action =
            self->q->shortcutSettings_->actionFor(symbols[i], modifiers);
        if (!action.isEmpty()) {
          self->q->handleShortcut(action);
          handled = true;
        }
      }
    }

    if (!handled && !state->virtualKeyboard)
      for (int i = 0; i < count; ++i)
        if (keypadSymbol(symbols[i])) {
          ++self->q->keypadKeyForwards_;
          break;
        }

    if (handled)
      return;

    if (self->inputMethod && self->inputMethod->method &&
        self->inputMethod->method->keyboard_grab &&
        !state->virtualKeyboard) {
      wlr_input_method_keyboard_grab_v2_send_key(
          self->inputMethod->method->keyboard_grab, event->time_msec,
          event->keycode, event->state);
      return;
    }

    wlr_seat_keyboard_notify_key(self->seat, event->time_msec, event->keycode,
                                 event->state);
    if (state->virtualKeyboard)
      self->restorePreferredKeyboard();
  }

  static void handleKeyboardModifiers(wl_listener *listener, void *) {
    auto *state = listenerOwner<KeyboardState>(listener);
    if (!state || !state->keyboard)
      return;
    auto *self = state->impl;
    wlr_seat_set_keyboard(self->seat, state->keyboard);
    if (self->inputMethod && self->inputMethod->method &&
        self->inputMethod->method->keyboard_grab &&
        !state->virtualKeyboard) {
      auto modifiers = state->keyboard->modifiers;
      wlr_input_method_keyboard_grab_v2_send_modifiers(
          self->inputMethod->method->keyboard_grab, &modifiers);
    } else {
      wlr_seat_keyboard_notify_modifiers(self->seat,
                                         &state->keyboard->modifiers);
    }
    if (state->virtualKeyboard)
      self->restorePreferredKeyboard();
  }

  static void handleKeyboardDestroy(wl_listener *listener, void *) {
    auto *state = listenerOwner<KeyboardState>(listener);
    if (!state)
      return;
    auto *self = state->impl;
    detachListener(state->key);
    detachListener(state->modifiers);
    detachListener(state->destroy);
    self->keyboards.removeAll(state);
    delete state;
    self->restorePreferredKeyboard();
    self->updateSeatCapabilities();
  }

  static void handleCursorMotion(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *event = static_cast<wlr_pointer_motion_event *>(data);
    if (!self || !event)
      return;
    wlr_cursor_move(self->cursor, &event->pointer->base, event->delta_x,
                    event->delta_y);
    self->processPointerMotion(event->time_msec);
  }

  static void handleCursorMotionAbsolute(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *event = static_cast<wlr_pointer_motion_absolute_event *>(data);
    if (!self || !event)
      return;
    wlr_cursor_warp_absolute(self->cursor, &event->pointer->base, event->x,
                             event->y);
    self->processPointerMotion(event->time_msec);
  }

  static void handleCursorButton(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *event = static_cast<wlr_pointer_button_event *>(data);
    if (!self || !event)
      return;
    wlr_seat_pointer_notify_button(self->seat, event->time_msec,
                                   event->button, event->state);
    if (event->state != WL_POINTER_BUTTON_STATE_PRESSED)
      return;

    double sx = 0;
    double sy = 0;
    wlr_surface *surface =
        self->surfaceAt(self->cursor->x, self->cursor->y, &sx, &sy);
    if (auto *client = self->clientForSurface(surface))
      self->q->focus(client);
  }

  static void handleCursorAxis(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *event = static_cast<wlr_pointer_axis_event *>(data);
    if (self && event)
      WlrootsCompat::notifyPointerAxis(self->seat, event);
  }

  static void handleCursorFrame(wl_listener *listener, void *) {
    auto *self = listenerOwner<Impl>(listener);
    if (self)
      wlr_seat_pointer_notify_frame(self->seat);
  }

  static void handleRequestCursor(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *event =
        static_cast<wlr_seat_pointer_request_set_cursor_event *>(data);
    if (!self || !event)
      return;
    if (self->seat->pointer_state.focused_client == event->seat_client)
      wlr_cursor_set_surface(self->cursor, event->surface, event->hotspot_x,
                             event->hotspot_y);
  }

  static void handleRequestSelection(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *event = static_cast<wlr_seat_request_set_selection_event *>(data);
    if (self && event)
      wlr_seat_set_selection(self->seat, event->source, event->serial);
  }

  static void handleKeyboardFocusChange(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *event = static_cast<wlr_seat_keyboard_focus_change_event *>(data);
    if (self && event)
      self->updateTextInputFocus(event->new_surface);
  }

  void addXdgToplevel(wlr_xdg_surface *surface,
                      wlr_xdg_toplevel *toplevel) {
    if (!surface || !toplevel)
      return;

    auto client = std::make_unique<ClientWindow>();
    client->id = q->nextWindowId_++;
    client->surface = surface;
    client->toplevel = toplevel;
    client->workspace = q->workspace_;
    client->sceneTree = wlr_scene_xdg_surface_create(normalLayer, surface);
    if (!client->sceneTree)
      return;

    client->sceneTree->node.data = client.get();
    if (surface->client && surface->client->client) {
      pid_t pid = 0;
      uid_t uid = 0;
      gid_t gid = 0;
      wl_client_get_credentials(surface->client->client, &pid, &uid, &gid);
      client->processId = static_cast<qint64>(pid);
    }

    auto *current = client.get();
    auto *state = new ToplevelState;
    state->impl = this;
    state->client = current;
    current->nativeState = state;
    q->clients_.push_back(std::move(client));
    q->updateClientMetadata(current);

    attachListener(&surface->surface->events.map, state->map, state,
                   handleToplevelMap);
    attachListener(&surface->surface->events.unmap, state->unmap, state,
                   handleToplevelUnmap);
    attachListener(&surface->surface->events.commit, state->commit, state,
                   handleToplevelCommit);
    attachListener(WlrootsCompat::xdgToplevelDestroySignal(surface, toplevel),
                   state->destroy, state, handleToplevelDestroy);
    attachListener(&toplevel->events.set_title, state->setTitle, state,
                   handleToplevelMetadata);
    attachListener(&toplevel->events.set_app_id, state->setAppId, state,
                   handleToplevelMetadata);
    attachListener(&toplevel->events.set_parent, state->setParent, state,
                   handleToplevelParent);
    attachListener(&toplevel->events.request_minimize, state->requestMinimize,
                   state, handleToplevelMinimize);
    attachListener(&toplevel->events.request_maximize, state->requestMaximize,
                   state, handleToplevelMaximize);
    attachListener(&toplevel->events.request_fullscreen,
                   state->requestFullscreen, state, handleToplevelFullscreen);
    wlr_scene_node_set_enabled(&current->sceneTree->node, false);
  }

#if WLR_VERSION_MINOR < 20
  static void handleNewXdgSurface(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *surface = static_cast<wlr_xdg_surface *>(data);
    if (!self || !surface || surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL ||
        !surface->toplevel)
      return;
    self->addXdgToplevel(surface, surface->toplevel);
  }
#else
  static void handleNewXdgToplevel(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *toplevel = static_cast<wlr_xdg_toplevel *>(data);
    if (!self || !toplevel || !toplevel->base)
      return;
    self->addXdgToplevel(toplevel->base, toplevel);
  }
#endif

  static void handleToplevelMap(wl_listener *listener, void *) {
    auto *state = listenerOwner<ToplevelState>(listener);
    if (!state || !state->client)
      return;
    auto *client = state->client;
    client->mapped = true;
    client->initialRuleApplied = true;
    state->impl->q->updateClientMetadata(client);
    state->impl->q->arrange();
    if (!client->utility)
      state->impl->q->focus(client);
  }

  static void handleToplevelUnmap(wl_listener *listener, void *) {
    auto *state = listenerOwner<ToplevelState>(listener);
    if (!state || !state->client)
      return;
    state->client->mapped = false;
    state->impl->q->tiling_.remove(state->client->id);
    if (state->impl->q->focused_ == state->client)
      state->impl->q->focused_ = nullptr;
    state->impl->q->arrange();
    state->impl->q->synchronizeTilingFocus();
  }

  static void handleToplevelCommit(wl_listener *listener, void *) {
    auto *state = listenerOwner<ToplevelState>(listener);
    if (!state || !state->client || !state->client->surface ||
        !state->client->toplevel)
      return;
    if (!state->client->surface->initial_commit)
      return;

    // Requests such as set_maximized can arrive before the first surface
    // commit. wlroots 0.20 asserts if a configure is scheduled before the
    // xdg_surface role is initialized, so remember the requested state and
    // apply it here once initialization has completed.
    wlr_xdg_toplevel_set_size(state->client->toplevel, 0, 0);
    if (state->client->toplevel->requested.maximized)
      wlr_xdg_toplevel_set_maximized(state->client->toplevel, true);
    if (state->client->toplevel->requested.fullscreen)
      wlr_xdg_toplevel_set_fullscreen(state->client->toplevel, true);
  }

  static void handleToplevelMetadata(wl_listener *listener, void *) {
    auto *state = listenerOwner<ToplevelState>(listener);
    if (!state || !state->client)
      return;
    state->impl->q->updateClientMetadata(state->client);
    state->impl->q->arrange();
  }

  static void handleToplevelParent(wl_listener *listener, void *) {
    auto *state = listenerOwner<ToplevelState>(listener);
    if (!state || !state->client)
      return;
    state->impl->q->updateClientMetadata(state->client);
    if (state->client->floating)
      state->impl->q->tiling_.remove(state->client->id);
    state->impl->q->arrange();
  }

  static void handleToplevelMinimize(wl_listener *listener, void *) {
    auto *state = listenerOwner<ToplevelState>(listener);
    if (!state || !state->client)
      return;
    state->client->minimized = true;
    state->impl->q->tiling_.setMinimized(state->client->id, true);
    state->impl->q->arrange();
    state->impl->q->synchronizeTilingFocus();
  }

  static void handleToplevelMaximize(wl_listener *listener, void *) {
    auto *state = listenerOwner<ToplevelState>(listener);
    if (!state || !state->client || !state->client->surface ||
        !state->client->surface->initialized)
      return;
    state->client->maximized = state->client->toplevel->requested.maximized;
    wlr_xdg_toplevel_set_maximized(state->client->toplevel,
                                   state->client->maximized);
    state->impl->q->arrange();
  }

  static void handleToplevelFullscreen(wl_listener *listener, void *) {
    auto *state = listenerOwner<ToplevelState>(listener);
    if (!state || !state->client || !state->client->surface ||
        !state->client->surface->initialized)
      return;
    const bool fullscreen = state->client->toplevel->requested.fullscreen;
    state->client->maximized = fullscreen;
    wlr_xdg_toplevel_set_fullscreen(state->client->toplevel, fullscreen);
    state->impl->q->arrange();
  }

  static void handleToplevelDestroy(wl_listener *listener, void *) {
    auto *state = listenerOwner<ToplevelState>(listener);
    if (!state || !state->client)
      return;
    auto *self = state->impl;
    ClientWindow *client = state->client;
    detachListener(state->map);
    detachListener(state->unmap);
    detachListener(state->commit);
    detachListener(state->destroy);
    detachListener(state->setTitle);
    detachListener(state->setAppId);
    detachListener(state->setParent);
    detachListener(state->requestMinimize);
    detachListener(state->requestMaximize);
    detachListener(state->requestFullscreen);
    client->nativeState = nullptr;
    self->q->removeClient(client);
    delete state;
  }

  static void handleNewLayerSurface(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *surface = static_cast<wlr_layer_surface_v1 *>(data);
    if (!self || !surface)
      return;
    if (!surface->output)
      surface->output = self->primaryOutput;

    wlr_scene_tree *parent = self->topLayer;
    switch (surface->pending.layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
      parent = self->backgroundLayer;
      break;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
      parent = self->bottomLayer;
      break;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
      parent = self->topLayer;
      break;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
      parent = self->overlayLayer;
      break;
    }

    auto *state = new LayerState;
    state->impl = self;
    state->surface = surface;
    state->sceneLayer = wlr_scene_layer_surface_v1_create(parent, surface);
    if (!state->sceneLayer) {
      delete state;
      return;
    }
    self->layers.append(state);
    attachListener(&surface->surface->events.map, state->map, state,
                   handleLayerMap);
    attachListener(&surface->surface->events.unmap, state->unmap, state,
                   handleLayerUnmap);
    attachListener(&surface->surface->events.commit, state->commit, state,
                   handleLayerCommit);
    attachListener(&surface->events.destroy, state->destroy, state,
                   handleLayerDestroy);
    // The first surface commit marks the role initialized; handleLayerCommit
    // will perform the initial configure/layout at that point.
  }

  static void handleLayerMap(wl_listener *listener, void *) {
    auto *state = listenerOwner<LayerState>(listener);
    if (!state)
      return;
    state->mapped = true;
    state->impl->arrangeLayers();
    state->impl->q->arrange();
    if (state->surface &&
        state->surface->current.keyboard_interactive !=
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
      state->impl->focusSurface(state->surface->surface);
  }

  static void handleLayerUnmap(wl_listener *listener, void *) {
    auto *state = listenerOwner<LayerState>(listener);
    if (!state)
      return;
    state->mapped = false;
    state->impl->arrangeLayers();
    state->impl->q->arrange();
  }

  static void handleLayerCommit(wl_listener *listener, void *) {
    auto *state = listenerOwner<LayerState>(listener);
    if (!state || !state->surface || !state->surface->initialized)
      return;
    state->impl->arrangeLayers();
    state->impl->q->arrange();
    if (state->mapped && state->surface &&
        state->surface->current.keyboard_interactive ==
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
      state->impl->focusSurface(state->surface->surface);
  }

  static void handleLayerDestroy(wl_listener *listener, void *) {
    auto *state = listenerOwner<LayerState>(listener);
    if (!state)
      return;
    auto *self = state->impl;
    detachListener(state->map);
    detachListener(state->unmap);
    detachListener(state->commit);
    detachListener(state->destroy);
    self->layers.removeAll(state);
    delete state;
    self->arrangeLayers();
    self->q->arrange();
  }

  static void handleNewInputMethod(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *method = static_cast<wlr_input_method_v2 *>(data);
    if (!self || !method)
      return;
    if (self->inputMethod) {
      wlr_input_method_v2_send_unavailable(method);
      return;
    }

    auto *state = new InputMethodState;
    state->impl = self;
    state->method = method;
    self->inputMethod = state;
    attachListener(&method->events.commit, state->commit, state,
                   handleInputMethodCommit);
    attachListener(&method->events.new_popup_surface, state->newPopup, state,
                   handleInputMethodPopup);
    attachListener(&method->events.grab_keyboard, state->grabKeyboard, state,
                   handleInputMethodGrab);
    attachListener(&method->events.destroy, state->destroy, state,
                   handleInputMethodDestroy);

    if (self->activeTextInput)
      self->syncTextInputToMethod(self->activeTextInput, true);
  }

  static void handleInputMethodCommit(wl_listener *listener, void *) {
    auto *state = listenerOwner<InputMethodState>(listener);
    if (!state || !state->method)
      return;
    auto *self = state->impl;
    if (!self->activeTextInput || !self->activeTextInput->text)
      return;

    auto *text = self->activeTextInput->text;
    const auto &current = state->method->current;
    if (current.preedit.text)
      wlr_text_input_v3_send_preedit_string(
          text, current.preedit.text, current.preedit.cursor_begin,
          current.preedit.cursor_end);
    if (current.commit_text)
      wlr_text_input_v3_send_commit_string(text, current.commit_text);
    if (current.delete_.before_length || current.delete_.after_length)
      wlr_text_input_v3_send_delete_surrounding_text(
          text, current.delete_.before_length, current.delete_.after_length);
    wlr_text_input_v3_send_done(text);
  }

  static void handleInputMethodPopup(wl_listener *listener, void *data) {
    auto *state = listenerOwner<InputMethodState>(listener);
    auto *popup = static_cast<wlr_input_popup_surface_v2 *>(data);
    if (!state || !popup)
      return;
    auto *self = state->impl;
    auto *popupState = new PopupState;
    popupState->impl = self;
    popupState->popup = popup;
    popupState->sceneTree =
        wlr_scene_subsurface_tree_create(self->overlayLayer, popup->surface);
    attachListener(&popup->events.destroy, popupState->destroy, popupState,
                   handleInputPopupDestroy);
    self->inputPopups.append(popupState);
    self->positionInputPopup(popupState);
  }

  static void handleInputPopupDestroy(wl_listener *listener, void *) {
    auto *state = listenerOwner<PopupState>(listener);
    if (!state)
      return;
    auto *self = state->impl;
    detachListener(state->destroy);
    self->inputPopups.removeAll(state);
    delete state;
  }

  static void handleInputMethodGrab(wl_listener *listener, void *data) {
    auto *state = listenerOwner<InputMethodState>(listener);
    auto *grab = static_cast<wlr_input_method_keyboard_grab_v2 *>(data);
    if (!state || !grab)
      return;
    if (auto *keyboard = state->impl->preferredKeyboard())
      wlr_input_method_keyboard_grab_v2_set_keyboard(grab, keyboard);
  }

  static void handleInputMethodDestroy(wl_listener *listener, void *) {
    auto *state = listenerOwner<InputMethodState>(listener);
    if (!state)
      return;
    auto *self = state->impl;
    detachListener(state->commit);
    detachListener(state->newPopup);
    detachListener(state->grabKeyboard);
    detachListener(state->destroy);
    if (self->inputMethod == state)
      self->inputMethod = nullptr;
    delete state;
  }

  static void handleNewTextInput(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *text = static_cast<wlr_text_input_v3 *>(data);
    if (!self || !text)
      return;
    auto *state = new TextInputState;
    state->impl = self;
    state->text = text;
    self->textInputs.append(state);
    attachListener(&text->events.enable, state->enable, state,
                   handleTextInputEnable);
    attachListener(&text->events.commit, state->commit, state,
                   handleTextInputCommit);
    attachListener(&text->events.disable, state->disable, state,
                   handleTextInputDisable);
    attachListener(&text->events.destroy, state->destroy, state,
                   handleTextInputDestroy);

    if (self->seat->keyboard_state.focused_surface)
      self->updateTextInputFocus(
          self->seat->keyboard_state.focused_surface);
  }

  static void handleTextInputEnable(wl_listener *listener, void *) {
    auto *state = listenerOwner<TextInputState>(listener);
    if (!state || !state->text || !state->text->focused_surface)
      return;
    state->impl->syncTextInputToMethod(state, true);
  }

  static void handleTextInputCommit(wl_listener *listener, void *) {
    auto *state = listenerOwner<TextInputState>(listener);
    if (!state || state->impl->activeTextInput != state)
      return;
    state->impl->syncTextInputToMethod(state, false);
    for (auto *popup : state->impl->inputPopups)
      state->impl->positionInputPopup(popup);
  }

  static void handleTextInputDisable(wl_listener *listener, void *) {
    auto *state = listenerOwner<TextInputState>(listener);
    if (state)
      state->impl->deactivateTextInput(state);
  }

  static void handleTextInputDestroy(wl_listener *listener, void *) {
    auto *state = listenerOwner<TextInputState>(listener);
    if (!state)
      return;
    auto *self = state->impl;
    self->deactivateTextInput(state);
    detachListener(state->enable);
    detachListener(state->commit);
    detachListener(state->disable);
    detachListener(state->destroy);
    self->textInputs.removeAll(state);
    delete state;
  }

  static void handleNewVirtualKeyboard(wl_listener *listener, void *data) {
    auto *self = listenerOwner<Impl>(listener);
    auto *keyboard = static_cast<wlr_virtual_keyboard_v1 *>(data);
    if (self && keyboard)
      self->addKeyboard(&keyboard->keyboard, true);
  }
};

WaylandCompositor::WaylandCompositor(const QByteArray &socket, bool fullscreen,
                                     bool startShell,
                                     const QString &rendererPreference)
    : d(std::make_unique<Impl>(this, socket, fullscreen, rendererPreference)) {
  shellModules_ = new ShellModules(this);
  systemStatus_ = new SystemStatus(this);
  audioSettings_ = new AudioSettings(this);
  powerSettings_ = new PowerSettings(this);
  sessionActions_ = new SessionActions(this);
  shortcutSettings_ = new ShortcutSettings;
  updateChecker_ = new UpdateChecker(this);
  networkStatus_ = new NetworkStatus(this);
  pluginManager_ = new PluginManager(this);
  pluginManager_->loadEnabled();

  if (!d->initialize())
    return;

  controlPath_ =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/" +
      QString::fromUtf8(socket) + "-control";
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
    if (xwayland_->start(environment, d->outputSize()) &&
        !xwayland_->startServer())
      qWarning("XWayland could not start; X11 clients are unavailable.");
  }

  publishSessionActivationEnvironment();

  if (startShell &&
      qEnvironmentVariableIntValue("LUNADASH_PUBLISH_ACTIVATION_ENV") == 1 &&
      qEnvironmentVariableIntValue("LUNADASH_DISABLE_FCITX") != 1) {
    const QString fcitx = QStandardPaths::findExecutable("fcitx5");
    if (!fcitx.isEmpty())
      spawn({"--replace"}, fcitx, false);
  }

  QObject::connect(shellModules_, &ShellModules::changed, this,
                   [this] { arrange(); });

  if (startShell) {
    if (auto *process = spawn({"--session"}, {}, false)) {
      connect(process, &QProcess::finished, this,
              [this](int code, QProcess::ExitStatus status) {
                if (shuttingDown_ || testStopping_)
                  return;
                if (status == QProcess::NormalExit && code == 0) {
                  requestShutdown();
                  return;
                }
                qWarning("LunaDash shell exited unexpectedly; restarting.");
                QTimer::singleShot(250, this, [this] {
                  if (!shuttingDown_ && !testStopping_)
                    spawn({"--session", "--no-welcome"}, {}, false);
                });
              });
    }
  }

  if (startShell && setupComplete()) {
    QTimer::singleShot(1200, this, [this] {
      if (testStopping_ || shuttingDown_)
        return;
      for (const auto &app :
           desktopPreferences().value("startupApps").toArray())
        spawn({"--app", app.toString()});
    });
  }

  qInfo().noquote() << "LunaDash compositor backend: wlroots"
                    << "socket:" << socket
                    << "input: libinput/xkbcommon"
                    << "QtWayland compositor: disabled";
}

WaylandCompositor::~WaylandCompositor() {
  shuttingDown_ = true;
  if (xwayland_) {
    xwayland_->stop();
    delete xwayland_;
    xwayland_ = nullptr;
  }
  for (auto *process : processes_) {
    if (!process || process->state() == QProcess::NotRunning)
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
          });

  if (program.isEmpty() && arguments.contains("--session")) {
    connect(process, &QProcess::started, this,
            [this, process] { shellProcessIds_.insert(process->processId()); });
    const QString sourceConfig =
        QStringLiteral(LUDASH_QML_SOURCE_DIR) + "/shell.qml";
    QString config =
        QFileInfo::exists(sourceConfig)
            ? sourceConfig
            : QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                     "lunadash/shell/shell.qml");
    if (config.isEmpty())
      config = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                      "ludash/shell/shell.qml");
    const QString quickshell = QStandardPaths::findExecutable("quickshell");
    if (quickshell.isEmpty()) {
      if (required)
        processFailure_ = true;
      qWarning("Quickshell is unavailable.");
      process->deleteLater();
      return nullptr;
    }
    process->start(quickshell, {"--path", config, "--no-color"});
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

void WaylandCompositor::launchExternalCommand(QStringList command) {
  if (command.isEmpty())
    return;

  const bool discord = isDiscordApplicationCommand(command);
  const bool chromium = isChromiumApplicationCommand(command);
  if (chromium) {
    command.erase(std::remove_if(command.begin(), command.end(),
                                 [](const QString &argument) {
                                   return argument.startsWith("--gtk-version=");
                                 }),
                  command.end());
    ensureWaylandChromiumFlags(command);

    if (discord) {
      // Electron/Discord on NVIDIA currently fails when Ozone Wayland selects
      // Vulkan. Keep the native Wayland path but force its GL renderer.
      command.erase(std::remove_if(command.begin(), command.end(),
                                   [](const QString &argument) {
                                     return argument.startsWith("--gtk-version=");
                                   }),
                    command.end());
      if (!command.contains("--disable-vulkan"))
        command.append("--disable-vulkan");
      if (!command.contains("--enable-wayland-ime"))
        command.append("--enable-wayland-ime");
      if (std::none_of(command.cbegin(), command.cend(),
                       [](const QString &argument) {
                         return argument.startsWith(
                             "--wayland-text-input-version=");
                       }))
        command.append("--wayland-text-input-version=3");
    }
  }

  const QString executable = command.takeFirst();
  spawn(command, executable, false);
}

bool WaylandCompositor::saveScreenshot(const QString &path) {
  if (!d || !d->primaryOutput || !path.startsWith('/'))
    return false;
  const QString grim = QStandardPaths::findExecutable("grim");
  if (grim.isEmpty()) {
    qWarning("Screenshot requested but grim is not installed.");
    return false;
  }

  wlr_output_schedule_frame(d->primaryOutput);
  QProcess process;
  process.setProcessEnvironment(clientEnvironment_);
  process.start(grim, {path});
  QElapsedTimer timer;
  timer.start();
  while (process.state() != QProcess::NotRunning && timer.elapsed() < 4000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    process.waitForFinished(10);
  }
  if (process.state() != QProcess::NotRunning)
    process.kill();
  return process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0 && QFileInfo::exists(path);
}

void WaylandCompositor::saveState(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return;
  file.write(QJsonDocument(state()).toJson(QJsonDocument::Indented));
}

bool WaylandCompositor::hasProcessFailure() const { return processFailure_; }

QRect WaylandCompositor::workArea() const {
  QRect area = d ? d->usableArea : QRect(0, 0, 1440, 900);
  const int gap = desktopPreferences().value("gap").toInt();
  area.adjust(gap, gap, -gap, -gap);
  if (area.width() < 1)
    area.setWidth(1);
  if (area.height() < 1)
    area.setHeight(1);
  return area;
}

void WaylandCompositor::updateClientMetadata(ClientWindow *client) {
  if (!client || !client->toplevel)
    return;
  client->title = safeUtf8(client->toplevel->title);
  client->appId = safeUtf8(client->toplevel->app_id);
  client->utility = isUtilityWindow(client->appId);
  client->iconName =
      client->utility ? QString() : windowIconName(client->appId, client->title);
  if (!client->initialRuleApplied) {
    const auto policy = initialWindowPolicy(client->appId, client->title);
    client->maximized = policy.maximized;
    client->floating = desktopPreferences().value("defaultFloating").toBool() ||
                       policy.floating || client->toplevel->parent;
    client->preferredFloatingSize = policy.floatingSize;
  } else if (client->toplevel->parent) {
    client->floating = true;
  }
}

void WaylandCompositor::configure(ClientWindow *client,
                                  const QRect &rectangle) {
  if (!client || !client->toplevel || !client->surface ||
      !client->surface->initialized || !client->sceneTree)
    return;
  client->geometry = rectangle;
  wlr_scene_node_set_position(&client->sceneTree->node, rectangle.x(),
                              rectangle.y());
  const QSize size(std::max(1, rectangle.width()),
                   std::max(1, rectangle.height()));
  if (client->lastSize != size) {
    client->lastSize = size;
    wlr_xdg_toplevel_set_size(client->toplevel, size.width(), size.height());
  }
  wlr_xdg_toplevel_set_maximized(client->toplevel, client->maximized);
}

void WaylandCompositor::arrange() {
  if (!d || !d->scene)
    return;
  const auto preferences = desktopPreferences();
  const int count = preferences.value("workspaceCount").toInt();
  workspace_ = std::clamp(workspace_, 0, std::max(0, count - 1));
  const QRect area = workArea();
  const int gap = preferences.value("gap").toInt();
  const int defaultColumnWidth =
      std::clamp(qRound(area.width() *
                        preferences.value("masterRatio").toInt() / 100.0),
                 1, std::max(1, area.width()));
  tiling_.setGap(gap);

  for (const auto &client : clients_) {
    client->workspace = std::min(client->workspace, count - 1);
    const bool tiled = client->mapped && !client->floating && !client->utility;
    if (tiled) {
      tiling_.insert(client->workspace, client->id, defaultColumnWidth);
      tiling_.moveToWorkspace(client->id, client->workspace);
      tiling_.setMinimized(client->id, client->minimized);
      if (client->maximized)
        tiling_.resize(client->id, area.width());
    } else {
      tiling_.remove(client->id);
    }

    const bool visible =
        client->mapped && !client->minimized &&
        client->workspace == workspace_ && !client->utility;
    if (client->sceneTree)
      wlr_scene_node_set_enabled(&client->sceneTree->node, visible);

    if (!visible)
      continue;
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
      configure(client.get(),
                client->manualGeometry.isValid() ? client->manualGeometry
                                                 : defaultGeometry);
    }
  }

  const auto placements = tiling_.layout(workspace_, area);
  for (const auto &placement : placements) {
    auto found = std::find_if(
        clients_.begin(), clients_.end(), [&placement](const auto &client) {
          return client->id == static_cast<int>(placement.window);
        });
    if (found == clients_.end() || (*found)->minimized ||
        (*found)->workspace != workspace_)
      continue;
    configure(found->get(), placement.geometry);
  }

  if (focused_ && focused_->sceneTree && focused_->mapped &&
      !focused_->minimized && focused_->workspace == workspace_)
    wlr_scene_node_raise_to_top(&focused_->sceneTree->node);

  d->updateBackground();
}

void WaylandCompositor::focus(ClientWindow *client) {
  if (!d || !client || !client->mapped || client->minimized ||
      client->workspace != workspace_ || !client->surface ||
      !client->surface->initialized)
    return;

  if (focused_ && focused_ != client && focused_->toplevel &&
      focused_->surface && focused_->surface->initialized)
    wlr_xdg_toplevel_set_activated(focused_->toplevel, false);

  focused_ = client;
  if (!client->floating)
    tiling_.focus(client->id);
  if (client->sceneTree)
    wlr_scene_node_raise_to_top(&client->sceneTree->node);
  wlr_xdg_toplevel_set_activated(client->toplevel, true);
  d->focusSurface(client->surface->surface);
}

void WaylandCompositor::focusNext(int direction) {
  QList<ClientWindow *> visible;
  for (const auto &client : clients_)
    if (client->mapped && !client->minimized && !client->utility &&
        client->workspace == workspace_)
      visible << client.get();
  if (visible.isEmpty()) {
    focused_ = nullptr;
    if (d && d->seat)
      wlr_seat_keyboard_notify_clear_focus(d->seat);
    return;
  }
  const qsizetype index = visible.indexOf(focused_);
  focus(visible[(index + direction + visible.size()) % visible.size()]);
}

void WaylandCompositor::synchronizeTilingFocus() {
  const auto target = tiling_.snapshot(workspace_).focusedWindow;
  if (target) {
    auto found = std::find_if(
        clients_.begin(), clients_.end(), [target](const auto &client) {
          return client->id == static_cast<int>(target) && client->mapped &&
                 !client->minimized;
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
  if (focused_ == client)
    focused_ = nullptr;
  tiling_.remove(client->id);
  std::erase_if(clients_, [client](const auto &entry) {
    return entry.get() == client;
  });
  arrange();
  synchronizeTilingFocus();
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

void WaylandCompositor::handleShortcut(const QString &action) {
  if (action.startsWith("workspace")) {
    bool ok = false;
    const int target = action.mid(QStringLiteral("workspace").size()).toInt(&ok);
    if (ok && target > 0 &&
        target <= desktopPreferences().value("workspaceCount").toInt()) {
      workspace_ = target - 1;
      arrange();
      synchronizeTilingFocus();
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
      tiling_.moveToWorkspace(focused_->id, focused_->workspace);
      arrange();
      synchronizeTilingFocus();
    }
    return;
  }

  if (action == "focusLeft")
    tiling_.focusLeft(workspace_);
  else if (action == "focusRight")
    tiling_.focusRight(workspace_);
  else if (action == "focusUp")
    tiling_.focusUp(workspace_);
  else if (action == "focusDown")
    tiling_.focusDown(workspace_);
  else if (action == "reorderLeft" && focused_)
    tiling_.reorder(focused_->id, -1);
  else if (action == "reorderRight" && focused_)
    tiling_.reorder(focused_->id, 1);
  else if (action == "groupLeft" && focused_) {
    const auto snap = tiling_.snapshot(workspace_);
    const auto current = std::find_if(
        snap.columns.cbegin(), snap.columns.cend(), [this](const auto &entry) {
          return entry.window == static_cast<TilingWindowId>(focused_->id);
        });
    if (current != snap.columns.cend() && current->columnIndex > 0) {
      const auto target = std::find_if(
          snap.columns.cbegin(), snap.columns.cend(), [current](const auto &e) {
            return e.columnIndex == current->columnIndex - 1;
          });
      if (target != snap.columns.cend())
        tiling_.groupWith(focused_->id, target->window);
    }
  } else if (action == "groupRight" && focused_) {
    const auto snap = tiling_.snapshot(workspace_);
    const auto current = std::find_if(
        snap.columns.cbegin(), snap.columns.cend(), [this](const auto &entry) {
          return entry.window == static_cast<TilingWindowId>(focused_->id);
        });
    if (current != snap.columns.cend()) {
      const auto target = std::find_if(
          snap.columns.cbegin(), snap.columns.cend(), [current](const auto &e) {
            return e.columnIndex == current->columnIndex + 1;
          });
      if (target != snap.columns.cend())
        tiling_.groupWith(focused_->id, target->window);
    }
  } else if (action == "expelWindow" && focused_)
    tiling_.expel(focused_->id);
  else if (action == "centerColumn" && focused_)
    tiling_.center(focused_->id, workArea());
  else if (action == "widenColumn" && focused_)
    tiling_.resize(focused_->id,
                   std::min(workArea().width(),
                            std::max(120, focused_->geometry.width() + 80)));
  else if (action == "narrowColumn" && focused_)
    tiling_.resize(focused_->id,
                   std::max(120, focused_->geometry.width() - 80));
  else if (action == "maximizeWindow" && focused_) {
    focused_->maximized = !focused_->maximized;
    if (focused_->toplevel)
      wlr_xdg_toplevel_set_maximized(focused_->toplevel,
                                     focused_->maximized);
  } else if ((action == "closeWindow" ||
              action == "closeWindowAlternate") &&
             focused_ && focused_->toplevel)
    wlr_xdg_toplevel_send_close(focused_->toplevel);
  else if (action == "minimizeWindow" && focused_) {
    focused_->minimized = true;
    tiling_.setMinimized(focused_->id, true);
  } else if (action == "toggleFloating" && focused_) {
    focused_->floating = !focused_->floating;
    if (focused_->floating)
      tiling_.remove(focused_->id);
  } else if (action == "launchTerminal")
    control({{"method", "launch-default"}, {"value", "terminal"}});
  else if (action == "launchFiles")
    control({{"method", "launch-default"}, {"value", "files"}});
  else if (action == "launchLauncher")
    spawn({"--app", "launcher"});
  else if (action == "screenshot")
    captureScreen();

  arrange();
  synchronizeTilingFocus();
}

QJsonObject WaylandCompositor::state() const {
  QJsonArray entries;
  for (const auto &client : clients_) {
    if (client->utility)
      continue;
    int bufferWidth = 0;
    int bufferHeight = 0;
    if (client->surface && client->surface->surface) {
      bufferWidth = client->surface->surface->current.width;
      bufferHeight = client->surface->surface->current.height;
    }
    entries.append(
        QJsonObject{{"contentWidth", bufferWidth},
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
                    {"manualResize", client->manualResize},
                    {"mapped", client->mapped}});
  }

  const auto tilingSnapshot = tiling_.snapshot(workspace_);
  QJsonArray tilingColumns;
  QJsonArray tilingGroups;
  QSet<int> emittedGroups;
  for (const auto &column : tilingSnapshot.columns) {
    tilingColumns.append(
        QJsonObject{{"window", static_cast<qint64>(column.window)},
                    {"width", column.width},
                    {"minimized", column.minimized},
                    {"focused", column.focused},
                    {"columnIndex", column.columnIndex},
                    {"rowIndex", column.rowIndex},
                    {"x", column.geometry.x()},
                    {"y", column.geometry.y()},
                    {"height", column.geometry.height()}});
    if (emittedGroups.contains(column.columnIndex))
      continue;
    emittedGroups.insert(column.columnIndex);
    QJsonArray members;
    for (auto id : column.columnMembers) {
      const auto found = std::find_if(
          clients_.cbegin(), clients_.cend(), [id](const auto &client) {
            return client->id == static_cast<int>(id);
          });
      members.append(QJsonObject{
          {"window", static_cast<qint64>(id)},
          {"title", found == clients_.cend() ? QString() : (*found)->title},
          {"appId", found == clients_.cend() ? QString() : (*found)->appId},
          {"icon", found == clients_.cend()
                       ? QStringLiteral("application-x-executable")
                       : (*found)->iconName}});
    }
    tilingGroups.append(
        QJsonObject{{"index", column.columnIndex},
                    {"focused", column.focused},
                    {"width", column.width},
                    {"members", members}});
  }

  const auto preferences = desktopPreferences();
  return {
      {"defaultApps", defaultApplications()},
      {"shellModules", shellModules_->snapshot()},
      {"panelExtent",
       shellModules_->panelExtent(preferences.value("panelHeight").toInt())},
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
      {"input",
       QJsonObject{{"layout", keyboardLayoutPreference(preferences)},
                   {"repeatRate", keyboardRepeatRate()},
                   {"repeatDelay", keyboardRepeatDelay()},
                   {"modifierResends", modifierResends_},
                   {"keypadKeyForwards", keypadKeyForwards_},
                   {"seatProtocolVersion",
                    WlrootsCompat::expectedSeatProtocolVersion()},
                   {"dataDeviceProtocolVersion", 3},
                   {"backend", "wlroots"},
                   {"qtInput", false},
                   {"inputMethodBridge",
                    d ? d->inputBridgeReady() : false}}},
      {"blurReady", false},
      {"blurFailed", false},
      {"blurFrames", 0},
      {"activeAnimations", 0},
      {"xwayland", xwayland_ ? xwayland_->snapshot() : QJsonObject{}},
      {"screenCapture",
       QJsonObject{{"protocol", "zwlr_screencopy_manager_v1"},
                   {"version", 3},
                   {"frames", 0},
                   {"lastCapture", lastCapture_},
                   {"error", captureError_}}},
      {"activationEnvironment",
       QJsonObject{{"published", activationEnvironmentPublished_},
                   {"error", activationEnvironmentError_}}},
      {"appearance", preferences},
      {"setupComplete", setupComplete()},
      {"network", networkStatus_->snapshot()},
      {"system", systemStatus_->snapshot()},
      {"wallpaperImage", wallpaperImageUrl()},
      {"workspace", workspace_},
      {"processFailure", processFailure_},
      {"clients", entries},
      {"tiling",
       QJsonObject{{"scrollOffset", tilingSnapshot.scrollOffset},
                   {"focusedWindow",
                    static_cast<qint64>(tilingSnapshot.focusedWindow)},
                   {"columns", tilingColumns},
                   {"groups", tilingGroups}}},
      {"layerSurfaces", d ? d->mappedLayerCount() : 0},
      {"language", selectedLanguage()},
      {"translations", languageDictionary(selectedLanguage())},
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
    workspace_ = number;
    arrange();
    synchronizeTilingFocus();
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
      return {{"error", "Could not write " + value}};
    lastCapture_ = value;
    captureError_.clear();
    return {{"path", value}};
  } else if (method == "screenshot") {
    captureScreen();
    if (!captureError_.isEmpty())
      return {{"error", captureError_}};
    return {{"path", lastCapture_}};
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
        "general", "appearance", "windows", "shortcuts", "display", "input",
        "sound",   "network",    "bluetooth", "power", "applications",
        "privacy", "system",     "devices",   "about", "modules"};
    if (!value.isEmpty() && !pages.contains(value))
      return {{"error", "Unknown settings page."}};
    settingsPage_ = value.isEmpty() ? "general" : value;
    settingsSerial_ = (settingsSerial_ + 1) % 1000000;
  } else if (method == "module-validate" || method == "module-save" ||
             method == "module-reset" || method == "module-code-trust" ||
             method == "module-template") {
    QString error;
    bool ok = false;
    if (method == "module-validate")
      ok = shellModules_->validate(value.toUtf8(), &error);
    else if (method == "module-save")
      ok = shellModules_->apply(value.toUtf8(), &error);
    else if (method == "module-reset")
      ok = shellModules_->reset(&error);
    else if (method == "module-template")
      ok = shellModules_->installTemplate(value, &error);
    else if (value == "true" || value == "false")
      ok = shellModules_->setCodeTrusted(value == "true", &error);
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
    const QUrl url(value);
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http"))
      return {{"error", "Only valid HTTP and HTTPS URLs can be opened."}};
    QString error;
    auto command = defaultApplicationCommand("browser", &error);
    if (!error.isEmpty())
      return {{"error", error}};
    command << url.toString(QUrl::FullyEncoded);
    launchExternalCommand(command);
  } else if (method == "launch-default" &&
             (value == "terminal" || value == "files" ||
              value == "browser")) {
    QString error;
    auto command = defaultApplicationCommand(value, &error);
    if (!error.isEmpty())
      return {{"error", error}};
    if (command.isEmpty()) {
      if (value == "browser")
        return {{"error", "No browser is available."}};
      spawn({"--app", value, "--builtin"});
    } else {
      launchExternalCommand(command);
    }
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
  } else if (method == "desktop-size") {
    QString error;
    if (!d || !d->resizePrimaryOutput(value, &error))
      return {{"error", error}};
  } else if (method == "reset-preferences") {
    QSettings settings;
    settings.remove("desktop");
    settings.sync();
    applyKeyboardConfiguration();
    arrange();
  } else if (method == "appearance") {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
    QString error;
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
      return {{"error", "Expected a JSON object of desktop preferences."}};
    if (!updateDesktopPreferences(document.object(), &error))
      return {{"error", error}};
    applyKeyboardConfiguration();
    if (d)
      d->updateBackground();
    arrange();
  } else if (method == "launch-x11") {
    QString error;
    if (!xwayland_ || !xwayland_->launch(QProcess::splitCommand(value), &error))
      return {{"error", error.isEmpty() ? "XWayland is unavailable." : error}};
  } else if (method == "launch-command") {
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isArray() || document.array().isEmpty())
      return {{"error", "Expected a non-empty command array."}};
    QStringList command;
    for (const auto &entry : document.array()) {
      if (!entry.isString() || entry.toString().size() > 1024)
        return {{"error", "Command arguments must be short strings."}};
      command << entry.toString();
    }
    launchExternalCommand(command);
  } else if (method == "finish-setup") {
    setSetupComplete(true);
  } else if (method == "setup") {
    setSetupComplete(false);
  } else if (method == "configure-network") {
    auto program = QStandardPaths::findExecutable("nm-connection-editor");
    QStringList arguments;
    if (program.isEmpty() &&
        !QStandardPaths::findExecutable("nmtui").isEmpty()) {
      for (const auto &terminal : {"konsole", "alacritty", "foot"}) {
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
    settingsPage_ = "appearance";
    settingsSerial_ = (settingsSerial_ + 1) % 1000000;
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
    if (!tiling_.groupWith(window, target))
      return {{"error", "Windows cannot be grouped."}};
    arrange();
  } else if (method == "expel-window") {
    const auto window = textWindowId(value);
    if (!window || !tiling_.expel(*window))
      return {{"error", "Window must be a member of a tiled group."}};
    arrange();
    synchronizeTilingFocus();
  } else if (method == "quit") {
    QTimer::singleShot(0, this, &WaylandCompositor::requestShutdown);
  } else if (method == "focus" || method == "close" ||
             method == "minimize") {
    const auto id = textWindowId(value);
    if (!id)
      return {{"error", "Window command requires a positive integer ID."}};
    for (const auto &client : clients_) {
      if (client->id != *id)
        continue;
      if (method == "close") {
        if (client->toplevel)
          wlr_xdg_toplevel_send_close(client->toplevel);
      } else if (method == "minimize") {
        client->minimized = true;
        tiling_.setMinimized(client->id, true);
        arrange();
        synchronizeTilingFocus();
      } else {
        workspace_ = client->workspace;
        client->minimized = false;
        tiling_.setMinimized(client->id, false);
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

void WaylandCompositor::publishSessionActivationEnvironment() {
  if (qEnvironmentVariableIntValue("LUNADASH_PUBLISH_ACTIVATION_ENV") != 1)
    return;
  auto environment = clientEnvironment_;
  if (xwayland_)
    xwayland_->applyEnvironment(environment);
  QString error;
  if (publishActivationEnvironment(environment, &error)) {
    activationEnvironmentPublished_ = true;
    return;
  }
  activationEnvironmentError_ =
      error.isEmpty()
          ? QStringLiteral("dbus-update-activation-environment failed.")
          : error;
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
  const QString path = nextCapturePath();
  if (path.isEmpty() || !saveScreenshot(path)) {
    captureError_ =
        "Could not write the screenshot. Install grim for wlroots capture.";
    return;
  }
  lastCapture_ = path;
  captureError_.clear();
}

void WaylandCompositor::closeTestSession(
    const std::function<void(bool)> &finished) {
  testStopping_ = true;
  for (const auto &client : clients_)
    if (client->toplevel)
      wlr_xdg_toplevel_send_close(client->toplevel);

  auto *timer = new QTimer(this);
  auto elapsed = std::make_shared<int>(0);
  connect(timer, &QTimer::timeout, this,
          [this, timer, elapsed, finished] {
            *elapsed += 50;
            if (clients_.empty() && xwayland_)
              xwayland_->stop();
            const bool xwaylandDone = !xwayland_ || xwayland_->stopped();
            const bool processesDone =
                std::all_of(processes_.cbegin(), processes_.cend(),
                            [](const QProcess *process) {
                              return !process ||
                                     process->state() == QProcess::NotRunning;
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
    if (!client->toplevel)
      continue;
    applications = true;
    wlr_xdg_toplevel_send_close(client->toplevel);
  }
  if (!applications)
    QCoreApplication::exit(hasProcessFailure() ? 2 : 0);
}

} // namespace LuDash
