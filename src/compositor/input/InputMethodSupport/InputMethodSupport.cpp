#include "compositor/input/InputMethodSupport/InputMethodSupport.hpp"

#include <QByteArray>
#include <QFile>
#include <QKeyEvent>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandOutput>
#include <QtWaylandCompositor/QWaylandQuickItem>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandSurface>

#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
#include <QtWaylandCompositor/private/qwaylandkeyboard_p.h>
#endif

#include "input-method-unstable-v2-server.h"
#include "text-input-unstable-v3-server.h"
#include "virtual-keyboard-unstable-v1-server.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xkbcommon/xkbcommon.h>

namespace LuDash {
namespace {

int createAnonymousFile(const QByteArray &contents) {
  const QString runtime =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (runtime.isEmpty())
    return -1;
  QByteArray name =
      QFile::encodeName(runtime + QStringLiteral("/lunadash-xkb-XXXXXX"));
  int fd = ::mkstemp(name.data());
  if (fd < 0)
    return -1;
  ::unlink(name.constData());
  const int flags = ::fcntl(fd, F_GETFD);
  if (flags < 0 || ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
    ::close(fd);
    return -1;
  }
  qsizetype written = 0;
  while (written < contents.size()) {
    const ssize_t result =
        ::write(fd, contents.constData() + written,
                static_cast<size_t>(contents.size() - written));
    if (result < 0) {
      if (errno == EINTR)
        continue;
      ::close(fd);
      return -1;
    }
    written += result;
  }
  ::lseek(fd, 0, SEEK_SET);
  return fd;
}

} // namespace

class InputMethodSupport::Impl {
private:
  struct InputMethodState;
  struct PopupState;
  struct VirtualKeyboardState;
  struct TextInputState;

public:
  Impl(InputMethodSupport *owner, QWaylandCompositor *compositor,
       QQuickWindow *window, QWaylandOutput *output)
      : owner_(owner), compositor_(compositor), window_(window), output_(output) {
    if (compositor_->isCreated()) {
      attachSeat();
      rebuildKeymap();
      installGlobals();
    }
    else
      QObject::connect(compositor_, &QWaylandCompositor::createdChanged, owner_,
              [this] {
                if (!compositor_->isCreated())
                  return;
                attachSeat();
                rebuildKeymap();
                installGlobals();
              });

    QTimer::singleShot(0, owner_, [this] { synchronizeActivation(); });
  }

  ~Impl() {
    if (textInputManagerGlobal_)
      wl_global_destroy(textInputManagerGlobal_);
    if (inputMethodManagerGlobal_)
      wl_global_destroy(inputMethodManagerGlobal_);
    if (virtualKeyboardManagerGlobal_)
      wl_global_destroy(virtualKeyboardManagerGlobal_);

    if (keymapFd_ >= 0)
      ::close(keymapFd_);
    if (xkbState_)
      xkb_state_unref(xkbState_);
    if (xkbKeymap_)
      xkb_keymap_unref(xkbKeymap_);
    if (xkbContext_)
      xkb_context_unref(xkbContext_);
  }

  bool nativeBridgeAvailable() const {
    return textInputManagerGlobal_ && inputMethodManagerGlobal_ &&
           virtualKeyboardManagerGlobal_;
  }

  bool hasKeyboardGrab() const {
    return textActive_ && inputMethod_ && inputMethod_->usable &&
           inputMethod_->keyboardGrab;
  }

  bool filterKeyEvent(QKeyEvent *event) {
    synchronizeActivation();
    if (!hasKeyboardGrab())
      return false;

    if (event->isAutoRepeat())
      return true;

    const uint nativeCode = static_cast<uint>(event->nativeScanCode());
    if (nativeCode < 8)
      return false;

    const uint32_t state =
        event->type() == QEvent::KeyRelease ? WL_KEYBOARD_KEY_STATE_RELEASED
                                            : WL_KEYBOARD_KEY_STATE_PRESSED;
    const uint32_t key = nativeCode - 8;
    zwp_input_method_keyboard_grab_v2_send_key(
        inputMethod_->keyboardGrab, compositor_->nextSerial(),
        compositor_->currentTimeMsecs(), key, state);

    if (xkbState_) {
      xkb_state_update_key(
          xkbState_, nativeCode,
          state == WL_KEYBOARD_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP);
      sendGrabModifiersIfChanged();
    }
    return true;
  }

private:
  struct InputMethodState {
    Impl *bridge = nullptr;
    wl_resource *resource = nullptr;
    wl_client *client = nullptr;
    bool usable = true;
    bool active = false;
    uint32_t doneSerial = 0;
    wl_resource *keyboardGrab = nullptr;
    QString pendingCommit;
    QString pendingPreedit;
    int32_t pendingPreeditBegin = 0;
    int32_t pendingPreeditEnd = 0;
    uint32_t deleteBefore = 0;
    uint32_t deleteAfter = 0;
  };

  struct PopupState {
    Impl *bridge = nullptr;
    InputMethodState *inputMethod = nullptr;
    wl_resource *resource = nullptr;
    QPointer<QWaylandSurface> surface;
    QPointer<QWaylandQuickItem> item;
  };

  struct VirtualKeyboardState {
    Impl *bridge = nullptr;
    wl_resource *resource = nullptr;
    bool hasKeymap = false;
  };

  struct TextInputState {
    Impl *bridge = nullptr;
    wl_resource *resource = nullptr;
    wl_client *client = nullptr;
    QPointer<QWaylandSurface> focus;
    bool pendingEnabled = false;
    bool enabled = false;
    QString pendingSurrounding;
    int32_t pendingCursor = 0;
    int32_t pendingAnchor = 0;
    uint32_t pendingChangeCause = 0;
    uint32_t pendingHints = 0;
    uint32_t pendingPurpose = 0;
    QRect pendingCursorRectangle;
    QString surrounding;
    int32_t cursor = 0;
    int32_t anchor = 0;
    uint32_t changeCause = 0;
    uint32_t hints = 0;
    uint32_t purpose = 0;
    QRect cursorRectangle;
    uint32_t serial = 0;
  };

  static void bindInputMethodManager(wl_client *client, void *data,
                                     uint32_t version, uint32_t id) {
    auto *self = static_cast<Impl *>(data);
    wl_resource *resource =
        wl_resource_create(client, &zwp_input_method_manager_v2_interface,
                           std::min<uint32_t>(version, 1), id);
    if (!resource) {
      wl_client_post_no_memory(client);
      return;
    }
    wl_resource_set_implementation(resource, &inputMethodManagerImpl_, self,
                                   nullptr);
  }

  static void getInputMethod(wl_client *client, wl_resource *managerResource,
                             wl_resource *seatResource, uint32_t id) {
    auto *self =
        static_cast<Impl *>(wl_resource_get_user_data(managerResource));
    if (!self)
      return;

    auto *seat = QWaylandSeat::fromSeatResource(seatResource);
    if (!seat || seat != self->compositor_->defaultSeat()) {
      wl_resource_post_error(managerResource, 0,
                             "invalid seat for zwp_input_method_v2");
      return;
    }

    wl_resource *resource =
        wl_resource_create(client, &zwp_input_method_v2_interface, 1, id);
    if (!resource) {
      wl_client_post_no_memory(client);
      return;
    }

    auto *state = new InputMethodState;
    state->bridge = self;
    state->resource = resource;
    state->client = client;
    wl_resource_set_implementation(resource, &inputMethodImpl_, state,
                                   destroyInputMethodResource);

    if (self->inputMethod_) {
      state->usable = false;
      zwp_input_method_v2_send_unavailable(resource);
      return;
    }

    self->inputMethod_ = state;
    self->inputMethodClient_ = client;
    self->synchronizeActivation();
  }

  static void destroyInputMethodManager(wl_client *, wl_resource *resource) {
    wl_resource_destroy(resource);
  }

  static void destroyInputMethodResource(wl_resource *resource) {
    auto *state =
        static_cast<InputMethodState *>(wl_resource_get_user_data(resource));
    if (!state)
      return;

    auto *self = state->bridge;
    if (state->keyboardGrab)
      wl_resource_destroy(state->keyboardGrab);

    if (self) {
      QList<wl_resource *> popupResources;
      for (auto it = self->popups_.cbegin(); it != self->popups_.cend(); ++it)
        if (it.value() && it.value()->inputMethod == state)
          popupResources.append(it.key());
      for (wl_resource *popup : popupResources)
        wl_resource_destroy(popup);

      if (self->inputMethod_ == state) {
        self->inputMethod_ = nullptr;
        self->inputMethodClient_ = nullptr;
        self->textActive_ = false;
      }
    }

    delete state;
  }

  static void inputMethodCommitString(wl_client *, wl_resource *resource,
                                      const char *text) {
    if (auto *state = inputMethodState(resource))
      state->pendingCommit = QString::fromUtf8(text ? text : "");
  }

  static void inputMethodSetPreedit(wl_client *, wl_resource *resource,
                                    const char *text, int32_t cursorBegin,
                                    int32_t cursorEnd) {
    if (auto *state = inputMethodState(resource)) {
      state->pendingPreedit = QString::fromUtf8(text ? text : "");
      state->pendingPreeditBegin = cursorBegin;
      state->pendingPreeditEnd = cursorEnd;
    }
  }

  static void inputMethodDeleteSurrounding(wl_client *, wl_resource *resource,
                                           uint32_t beforeLength,
                                           uint32_t afterLength) {
    if (auto *state = inputMethodState(resource)) {
      state->deleteBefore = beforeLength;
      state->deleteAfter = afterLength;
    }
  }

  static void inputMethodCommit(wl_client *, wl_resource *resource,
                                uint32_t serial) {
    auto *state = inputMethodState(resource);
    if (!state || !state->bridge || !state->usable)
      return;
    state->bridge->applyInputMethodCommit(state, serial);
  }

  static void inputMethodGetPopup(wl_client *, wl_resource *resource,
                                  uint32_t id, wl_resource *surfaceResource) {
    auto *state = inputMethodState(resource);
    if (!state || !state->bridge || !state->usable)
      return;
    state->bridge->createPopup(state, id, surfaceResource);
  }

  static void inputMethodGrabKeyboard(wl_client *client, wl_resource *resource,
                                      uint32_t id) {
    auto *state = inputMethodState(resource);
    if (!state || !state->bridge || !state->usable)
      return;

    if (state->keyboardGrab) {
      wl_resource_post_error(resource, 0,
                             "zwp_input_method_v2 already has a keyboard grab");
      return;
    }

    wl_resource *grab = wl_resource_create(
        client, &zwp_input_method_keyboard_grab_v2_interface, 1, id);
    if (!grab) {
      wl_client_post_no_memory(client);
      return;
    }
    state->keyboardGrab = grab;
    wl_resource_set_implementation(grab, &keyboardGrabImpl_, state,
                                   destroyKeyboardGrabResource);
    state->bridge->sendGrabInitialState(state);
  }

  static void inputMethodDestroy(wl_client *, wl_resource *resource) {
    wl_resource_destroy(resource);
  }

  static void keyboardGrabRelease(wl_client *, wl_resource *resource) {
    wl_resource_destroy(resource);
  }

  static void destroyKeyboardGrabResource(wl_resource *resource) {
    auto *state =
        static_cast<InputMethodState *>(wl_resource_get_user_data(resource));
    if (state && state->keyboardGrab == resource)
      state->keyboardGrab = nullptr;
  }

  static InputMethodState *inputMethodState(wl_resource *resource) {
    return static_cast<InputMethodState *>(wl_resource_get_user_data(resource));
  }

  static void popupDestroy(wl_client *, wl_resource *resource) {
    wl_resource_destroy(resource);
  }

  static void destroyPopupResource(wl_resource *resource) {
    auto *popup = static_cast<PopupState *>(wl_resource_get_user_data(resource));
    if (!popup)
      return;
    if (popup->bridge)
      popup->bridge->popups_.remove(resource);
    if (popup->item)
      popup->item->deleteLater();
    delete popup;
  }

  static void bindVirtualKeyboardManager(wl_client *client, void *data,
                                         uint32_t version, uint32_t id) {
    auto *self = static_cast<Impl *>(data);
    wl_resource *resource = wl_resource_create(
        client, &zwp_virtual_keyboard_manager_v1_interface,
        std::min<uint32_t>(version, 1), id);
    if (!resource) {
      wl_client_post_no_memory(client);
      return;
    }
    wl_resource_set_implementation(resource, &virtualKeyboardManagerImpl_, self,
                                   nullptr);
  }

  static void createVirtualKeyboard(wl_client *client,
                                    wl_resource *managerResource,
                                    wl_resource *seatResource, uint32_t id) {
    auto *self =
        static_cast<Impl *>(wl_resource_get_user_data(managerResource));
    if (!self)
      return;

    auto *seat = QWaylandSeat::fromSeatResource(seatResource);
    if (!seat || seat != self->compositor_->defaultSeat()) {
      wl_resource_post_error(managerResource, 0,
                             "invalid seat for virtual keyboard");
      return;
    }

    if (!self->inputMethodClient_ || self->inputMethodClient_ != client) {
      wl_resource_post_error(
          managerResource, 0,
          "virtual keyboard is restricted to the active input method client");
      return;
    }

    wl_resource *resource =
        wl_resource_create(client, &zwp_virtual_keyboard_v1_interface, 1, id);
    if (!resource) {
      wl_client_post_no_memory(client);
      return;
    }

    auto *state = new VirtualKeyboardState;
    state->bridge = self;
    state->resource = resource;
    self->virtualKeyboards_.insert(resource, state);
    wl_resource_set_implementation(resource, &virtualKeyboardImpl_, state,
                                   destroyVirtualKeyboardResource);
  }

  static void virtualKeyboardKeymap(wl_client *, wl_resource *resource,
                                    uint32_t format, int32_t fd,
                                    uint32_t size) {
    Q_UNUSED(size);
    auto *state = static_cast<VirtualKeyboardState *>(
        wl_resource_get_user_data(resource));
    if (!state) {
      ::close(fd);
      return;
    }

    state->hasKeymap = format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1;
    ::close(fd);
    if (!state->hasKeymap)
      wl_resource_post_error(resource, 0,
                             "virtual keyboard requires an XKB keymap");
  }

  static void virtualKeyboardKey(wl_client *, wl_resource *resource,
                                 uint32_t, uint32_t key, uint32_t stateValue) {
    auto *state = static_cast<VirtualKeyboardState *>(
        wl_resource_get_user_data(resource));
    if (!state || !state->bridge)
      return;
    if (!state->hasKeymap) {
      wl_resource_post_error(resource, 0,
                             "virtual keyboard key received before keymap");
      return;
    }

    auto *keyboard = state->bridge->compositor_->defaultSeat()->keyboard();
    if (!keyboard)
      return;

    const uint nativeScanCode = key + 8;
    if (stateValue == WL_KEYBOARD_KEY_STATE_PRESSED)
      keyboard->sendKeyPressEvent(nativeScanCode);
    else if (stateValue == WL_KEYBOARD_KEY_STATE_RELEASED)
      keyboard->sendKeyReleaseEvent(nativeScanCode);
  }

  static void virtualKeyboardModifiers(wl_client *, wl_resource *resource,
                                       uint32_t depressed, uint32_t latched,
                                       uint32_t locked, uint32_t group) {
    auto *state = static_cast<VirtualKeyboardState *>(
        wl_resource_get_user_data(resource));
    if (!state || !state->bridge || !state->hasKeymap)
      return;

#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)
    auto *seat = state->bridge->compositor_->defaultSeat();
    auto *keyboard = seat ? seat->keyboard() : nullptr;
    auto *focus = seat ? seat->keyboardFocus() : nullptr;
    if (!keyboard || !focus)
      return;

    auto *d = QWaylandKeyboardPrivate::get(keyboard);
    if (!d)
      return;
    d->modsDepressed = depressed;
    d->modsLatched = latched;
    d->modsLocked = locked;
    d->group = group;
    auto *target = d->resourceMap().value(focus->waylandClient(), nullptr);
    if (target)
      d->send_modifiers(target->handle,
                        state->bridge->compositor_->nextSerial(), depressed,
                        latched, locked, group);
#else
    Q_UNUSED(depressed);
    Q_UNUSED(latched);
    Q_UNUSED(locked);
    Q_UNUSED(group);
#endif
  }

  static void virtualKeyboardDestroy(wl_client *, wl_resource *resource) {
    wl_resource_destroy(resource);
  }

  static void destroyVirtualKeyboardResource(wl_resource *resource) {
    auto *state = static_cast<VirtualKeyboardState *>(
        wl_resource_get_user_data(resource));
    if (!state)
      return;
    if (state->bridge)
      state->bridge->virtualKeyboards_.remove(resource);
    delete state;
  }

  static void bindTextInputManager(wl_client *client, void *data,
                                   uint32_t version, uint32_t id) {
    auto *self = static_cast<Impl *>(data);
    wl_resource *resource =
        wl_resource_create(client, &zwp_text_input_manager_v3_interface,
                           std::min<uint32_t>(version, 1), id);
    if (!resource) {
      wl_client_post_no_memory(client);
      return;
    }
    wl_resource_set_implementation(resource, &textInputManagerImpl_, self,
                                   nullptr);
  }

  static void destroyTextInputManager(wl_client *, wl_resource *resource) {
    wl_resource_destroy(resource);
  }

  static void getTextInput(wl_client *client, wl_resource *managerResource,
                           uint32_t id, wl_resource *seatResource) {
    auto *self =
        static_cast<Impl *>(wl_resource_get_user_data(managerResource));
    if (!self)
      return;
    auto *seat = QWaylandSeat::fromSeatResource(seatResource);
    if (!seat || seat != self->compositor_->defaultSeat()) {
      wl_resource_post_error(managerResource, 0,
                             "invalid seat for zwp_text_input_v3");
      return;
    }
    wl_resource *resource =
        wl_resource_create(client, &zwp_text_input_v3_interface, 1, id);
    if (!resource) {
      wl_client_post_no_memory(client);
      return;
    }
    auto *state = new TextInputState;
    state->bridge = self;
    state->resource = resource;
    state->client = client;
    self->textInputs_.insert(resource, state);
    wl_resource_set_implementation(resource, &textInputImpl_, state,
                                   destroyTextInputResource);
    QWaylandSurface *focus = seat->keyboardFocus();
    if (focus && focus->waylandClient() == client) {
      state->focus = focus;
      zwp_text_input_v3_send_enter(resource, focus->resource());
    }
  }

  static TextInputState *textInputState(wl_resource *resource) {
    return static_cast<TextInputState *>(wl_resource_get_user_data(resource));
  }

  static void resetPendingTextInput(TextInputState *state, bool enabled) {
    if (!state)
      return;
    state->pendingEnabled = enabled;
    state->pendingSurrounding.clear();
    state->pendingCursor = 0;
    state->pendingAnchor = 0;
    state->pendingChangeCause = 0;
    state->pendingHints = 0;
    state->pendingPurpose = 0;
    state->pendingCursorRectangle = {};
  }

  static void destroyTextInputResource(wl_resource *resource) {
    auto *state = textInputState(resource);
    if (!state)
      return;
    if (state->bridge) {
      if (state->bridge->activeTextInput_ == state)
        state->bridge->activeTextInput_ = nullptr;
      state->bridge->textInputs_.remove(resource);
      state->bridge->synchronizeActivation();
      state->bridge->updatePopups();
    }
    delete state;
  }

  static void textInputDestroy(wl_client *, wl_resource *resource) {
    wl_resource_destroy(resource);
  }

  static void textInputEnable(wl_client *, wl_resource *resource) {
    auto *state = textInputState(resource);
    if (state && state->focus)
      resetPendingTextInput(state, true);
  }

  static void textInputDisable(wl_client *, wl_resource *resource) {
    auto *state = textInputState(resource);
    if (state && state->focus)
      resetPendingTextInput(state, false);
  }

  static void textInputSetSurrounding(wl_client *, wl_resource *resource,
                                      const char *text, int32_t cursor,
                                      int32_t anchor) {
    auto *state = textInputState(resource);
    if (!state || !state->focus || !state->pendingEnabled)
      return;
    state->pendingSurrounding = QString::fromUtf8(text ? text : "");
    const int32_t bytes =
        static_cast<int32_t>(state->pendingSurrounding.toUtf8().size());
    state->pendingCursor = std::clamp(cursor, int32_t(0), bytes);
    state->pendingAnchor = std::clamp(anchor, int32_t(0), bytes);
  }

  static void textInputSetChangeCause(wl_client *, wl_resource *resource,
                                      uint32_t cause) {
    auto *state = textInputState(resource);
    if (state && state->focus && state->pendingEnabled)
      state->pendingChangeCause = cause;
  }

  static void textInputSetContentType(wl_client *, wl_resource *resource,
                                      uint32_t hints, uint32_t purpose) {
    auto *state = textInputState(resource);
    if (!state || !state->focus || !state->pendingEnabled)
      return;
    state->pendingHints = hints;
    state->pendingPurpose = purpose;
  }

  static void textInputSetCursorRectangle(wl_client *, wl_resource *resource,
                                          int32_t x, int32_t y, int32_t width,
                                          int32_t height) {
    auto *state = textInputState(resource);
    if (!state || !state->focus || !state->pendingEnabled)
      return;
    state->pendingCursorRectangle =
        QRect(x, y, std::max(0, width), std::max(0, height));
  }

  static void textInputCommit(wl_client *, wl_resource *resource) {
    auto *state = textInputState(resource);
    if (!state || !state->focus || !state->bridge)
      return;
    ++state->serial;
    state->enabled = state->pendingEnabled;
    if (state->enabled) {
      state->surrounding = state->pendingSurrounding;
      state->cursor = state->pendingCursor;
      state->anchor = state->pendingAnchor;
      state->changeCause = state->pendingChangeCause;
      state->hints = state->pendingHints;
      state->purpose = state->pendingPurpose;
      state->cursorRectangle = state->pendingCursorRectangle;
    } else {
      state->surrounding.clear();
      state->cursor = 0;
      state->anchor = 0;
      state->changeCause = 0;
      state->hints = 0;
      state->purpose = 0;
      state->cursorRectangle = {};
    }
    state->bridge->synchronizeActivation();
    if (state->bridge->activeTextInput_ == state)
      state->bridge->sendTextState();
    zwp_text_input_v3_send_done(resource, state->serial);
    state->bridge->updatePopups();
  }

  static void textInputSetAvailableActions(wl_client *, wl_resource *,
                                           wl_array *) {}
  static void textInputShowPanel(wl_client *, wl_resource *) {}
  static void textInputHidePanel(wl_client *, wl_resource *) {}

  void attachSeat() {
    if (seatConnected_)
      return;
    auto *seat = compositor_->defaultSeat();
    if (!seat)
      return;
    seatConnected_ = true;
    QObject::connect(
        seat, &QWaylandSeat::keyboardFocusChanged, owner_,
        [this](QWaylandSurface *surface, QWaylandSurface *oldSurface) {
          for (auto *state : std::as_const(textInputs_)) {
            if (state && state->focus && state->focus == oldSurface) {
              zwp_text_input_v3_send_leave(state->resource,
                                           oldSurface->resource());
              state->focus = nullptr;
              state->enabled = false;
              resetPendingTextInput(state, false);
            }
          }
          if (surface) {
            for (auto *state : std::as_const(textInputs_)) {
              if (!state || state->client != surface->waylandClient())
                continue;
              state->focus = surface;
              zwp_text_input_v3_send_enter(state->resource,
                                           surface->resource());
            }
          }
          QTimer::singleShot(0, owner_, [this] {
            synchronizeActivation();
            updatePopups();
          });
        });
  }

  void installGlobals() {
    if (textInputManagerGlobal_ || inputMethodManagerGlobal_ ||
        virtualKeyboardManagerGlobal_)
      return;
    textInputManagerGlobal_ =
        wl_global_create(compositor_->display(),
                         &zwp_text_input_manager_v3_interface, 1, this,
                         bindTextInputManager);
    inputMethodManagerGlobal_ = wl_global_create(
        compositor_->display(), &zwp_input_method_manager_v2_interface, 1, this,
        bindInputMethodManager);
    virtualKeyboardManagerGlobal_ = wl_global_create(
        compositor_->display(), &zwp_virtual_keyboard_manager_v1_interface, 1,
        this, bindVirtualKeyboardManager);
    if (!textInputManagerGlobal_ || !inputMethodManagerGlobal_ ||
        !virtualKeyboardManagerGlobal_) {
      qWarning("LunaDash: failed to publish native input-method globals.");
      return;
    }
    qInfo("LunaDash input method: text-input-v3 + input-method-v2 + virtual-keyboard-v1 bridge ready");
  }

  TextInputState *activeTextInput() const {
    auto *seat = compositor_->defaultSeat();
    QWaylandSurface *focus = seat ? seat->keyboardFocus() : nullptr;
    if (!focus)
      return nullptr;
    for (auto *state : std::as_const(textInputs_))
      if (state && state->enabled && state->focus == focus &&
          state->client == focus->waylandClient())
        return state;
    return nullptr;
  }

  QWaylandSurface *textFocus() const {
    auto *state = activeTextInput();
    return state ? state->focus.data() : nullptr;
  }

  void synchronizeActivation() {
    TextInputState *next = activeTextInput();
    const bool active = next != nullptr;
    const bool changed = activeTextInput_ != next;
    if (changed && inputMethod_ && inputMethod_->usable &&
        inputMethod_->active) {
      zwp_input_method_v2_send_deactivate(inputMethod_->resource);
      inputMethod_->active = false;
      sendDone(inputMethod_);
    }
    activeTextInput_ = next;
    textActive_ = active;
    if (!inputMethod_ || !inputMethod_->usable)
      return;
    if (active && !inputMethod_->active) {
      zwp_input_method_v2_send_activate(inputMethod_->resource);
      inputMethod_->active = true;
      sendTextState();
    } else if (active) {
      sendTextState();
    } else if (inputMethod_->active) {
      zwp_input_method_v2_send_deactivate(inputMethod_->resource);
      inputMethod_->active = false;
      sendDone(inputMethod_);
    }
  }

  void sendTextState() {
    auto *text = activeTextInput_;
    if (!text || !textActive_ || !inputMethod_ || !inputMethod_->usable ||
        !inputMethod_->active)
      return;
    const QByteArray utf8 = text->surrounding.toUtf8();
    zwp_input_method_v2_send_surrounding_text(
        inputMethod_->resource, utf8.constData(),
        static_cast<uint32_t>(std::max(0, text->cursor)),
        static_cast<uint32_t>(std::max(0, text->anchor)));
    zwp_input_method_v2_send_text_change_cause(inputMethod_->resource,
                                               text->changeCause);
    zwp_input_method_v2_send_content_type(inputMethod_->resource, text->hints,
                                          text->purpose);
    sendDone(inputMethod_);
  }

  void sendDone(InputMethodState *state) {
    if (!state || !state->resource || !state->usable)
      return;
    zwp_input_method_v2_send_done(state->resource);
    ++state->doneSerial;
  }

  void applyInputMethodCommit(InputMethodState *state, uint32_t serial) {
    auto *text = activeTextInput_;
    if (!state || !text || !textActive_)
      return;
    if (serial != state->doneSerial) {
      clearPending(state);
      return;
    }
    if (state->deleteBefore || state->deleteAfter)
      zwp_text_input_v3_send_delete_surrounding_text(
          text->resource, state->deleteBefore, state->deleteAfter);
    const QByteArray preedit = state->pendingPreedit.toUtf8();
    zwp_text_input_v3_send_preedit_string(
        text->resource, state->pendingPreedit.isNull() ? nullptr
                                                      : preedit.constData(),
        state->pendingPreeditBegin, state->pendingPreeditEnd);
    if (!state->pendingCommit.isNull()) {
      const QByteArray commit = state->pendingCommit.toUtf8();
      zwp_text_input_v3_send_commit_string(text->resource,
                                           commit.constData());
    }
    zwp_text_input_v3_send_done(text->resource, text->serial);
    clearPending(state);
    updatePopups();
  }

  static void clearPending(InputMethodState *state) {
    state->pendingCommit = QString();
    state->pendingPreedit = QString();
    state->pendingPreeditBegin = 0;
    state->pendingPreeditEnd = 0;
    state->deleteBefore = 0;
    state->deleteAfter = 0;
  }

  void createPopup(InputMethodState *state, uint32_t id,
                   wl_resource *surfaceResource) {
    auto *surface = QWaylandSurface::fromResource(surfaceResource);
    if (!surface) {
      wl_resource_post_error(state->resource, 0,
                             "invalid wl_surface for input popup");
      return;
    }
    static QWaylandSurfaceRole inputPopupRole(
        QByteArrayLiteral("zwp_input_popup_surface_v2"));
    if (!surface->setRole(&inputPopupRole, state->resource, 0))
      return;
    wl_resource *resource = wl_resource_create(
        state->client, &zwp_input_popup_surface_v2_interface, 1, id);
    if (!resource) {
      wl_client_post_no_memory(state->client);
      return;
    }
    auto *popup = new PopupState;
    popup->bridge = this;
    popup->inputMethod = state;
    popup->resource = resource;
    popup->surface = surface;
    popup->item = new QWaylandQuickItem(window_->contentItem());
    popup->item->setSurface(surface);
    popup->item->setOutput(output_);
    popup->item->setFocusOnClick(false);
    popup->item->setInputEventsEnabled(true);
    popup->item->setZ(1000);
    popup->item->setVisible(false);
    popups_.insert(resource, popup);
    wl_resource_set_implementation(resource, &inputPopupImpl_, popup,
                                   destroyPopupResource);
    QObject::connect(surface, &QWaylandSurface::destinationSizeChanged, owner_,
                     [this, resource] {
                       if (auto *popup = popups_.value(resource, nullptr))
                         updatePopup(popup);
                     });
    QObject::connect(surface, &QWaylandSurface::hasContentChanged, owner_,
                     [this, resource] {
                       if (auto *popup = popups_.value(resource, nullptr))
                         updatePopup(popup);
                     });
    updatePopup(popup);
  }

  void updatePopups() {
    for (auto *popup : std::as_const(popups_))
      updatePopup(popup);
  }

  void updatePopup(PopupState *popup) {
    if (!popup || !popup->item || !popup->surface)
      return;
    auto *text = activeTextInput_;
    QWaylandSurface *focus = text ? text->focus.data() : nullptr;
    if (!textActive_ || !text || !focus || !focus->primaryView()) {
      popup->item->setVisible(false);
      return;
    }
    auto *focusItem =
        qobject_cast<QWaylandQuickItem *>(focus->primaryView()->renderObject());
    if (!focusItem) {
      popup->item->setVisible(false);
      return;
    }
    QRect cursor = text->cursorRectangle;
    if (!cursor.isValid())
      cursor = QRect(0, 0, 1, 1);
    const QPointF cursorTopLeft =
        focusItem->mapToScene(focusItem->mapFromSurface(cursor.topLeft()));
    const QPointF cursorBottomLeft =
        focusItem->mapToScene(focusItem->mapFromSurface(
            QPointF(cursor.left(), cursor.bottom() + 1)));
    QSizeF popupSize = popup->surface->destinationSize();
    if (popupSize.isEmpty())
      popupSize = QSizeF(std::max<qreal>(1, popup->item->width()),
                         std::max<qreal>(1, popup->item->height()));
    qreal x = cursorBottomLeft.x();
    qreal y = cursorBottomLeft.y();
    if (x + popupSize.width() > window_->width())
      x = std::max<qreal>(0, window_->width() - popupSize.width());
    if (y + popupSize.height() > window_->height())
      y = std::max<qreal>(0, cursorTopLeft.y() - popupSize.height());
    const QPointF scenePosition(x, y);
    popup->item->setPosition(
        popup->item->parentItem()->mapFromScene(scenePosition));
    popup->item->setVisible(popup->surface->hasContent());
    const QPointF relativeTopLeft = cursorTopLeft - scenePosition;
    zwp_input_popup_surface_v2_send_text_input_rectangle(
        popup->resource, qRound(relativeTopLeft.x()),
        qRound(relativeTopLeft.y()), std::max(1, cursor.width()),
        std::max(1, cursor.height()));
  }

  void sendGrabInitialState(InputMethodState *state) {
    if (!state || !state->keyboardGrab)
      return;

    if (keymapFd_ >= 0 && keymapSize_ > 0)
      zwp_input_method_keyboard_grab_v2_send_keymap(
          state->keyboardGrab, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymapFd_,
          keymapSize_);

    auto *keyboard = compositor_->defaultSeat()->keyboard();
    if (keyboard)
      zwp_input_method_keyboard_grab_v2_send_repeat_info(
          state->keyboardGrab, static_cast<int32_t>(keyboard->repeatRate()),
          static_cast<int32_t>(keyboard->repeatDelay()));

    lastDepressed_ = UINT32_MAX;
    lastLatched_ = UINT32_MAX;
    lastLocked_ = UINT32_MAX;
    lastGroup_ = UINT32_MAX;
    sendGrabModifiersIfChanged();
  }

  void sendGrabModifiersIfChanged() {
    if (!hasKeyboardGrab() || !xkbState_)
      return;

    const uint32_t depressed =
        xkb_state_serialize_mods(xkbState_, XKB_STATE_MODS_DEPRESSED);
    const uint32_t latched =
        xkb_state_serialize_mods(xkbState_, XKB_STATE_MODS_LATCHED);
    const uint32_t locked =
        xkb_state_serialize_mods(xkbState_, XKB_STATE_MODS_LOCKED);
    const uint32_t group =
        xkb_state_serialize_layout(xkbState_, XKB_STATE_LAYOUT_EFFECTIVE);

    if (depressed == lastDepressed_ && latched == lastLatched_ &&
        locked == lastLocked_ && group == lastGroup_)
      return;

    lastDepressed_ = depressed;
    lastLatched_ = latched;
    lastLocked_ = locked;
    lastGroup_ = group;
    zwp_input_method_keyboard_grab_v2_send_modifiers(
        inputMethod_->keyboardGrab, compositor_->nextSerial(), depressed,
        latched, locked, group);
  }

  void rebuildKeymap() {
    if (keymapFd_ >= 0) {
      ::close(keymapFd_);
      keymapFd_ = -1;
    }
    if (xkbState_) {
      xkb_state_unref(xkbState_);
      xkbState_ = nullptr;
    }
    if (xkbKeymap_) {
      xkb_keymap_unref(xkbKeymap_);
      xkbKeymap_ = nullptr;
    }
    if (!xkbContext_)
      xkbContext_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!xkbContext_)
      return;

    QWaylandKeymap *qtKeymap =
        compositor_->defaultSeat() ? compositor_->defaultSeat()->keymap()
                                   : nullptr;
    QByteArray rules;
    QByteArray model;
    QByteArray layout;
    QByteArray variant;
    QByteArray options;
    if (qtKeymap) {
      rules = qtKeymap->rules().toUtf8();
      model = qtKeymap->model().toUtf8();
      layout = qtKeymap->layout().toUtf8();
      variant = qtKeymap->variant().toUtf8();
      options = qtKeymap->options().toUtf8();
    }

    xkb_rule_names names{};
    names.rules = rules.isEmpty() ? nullptr : rules.constData();
    names.model = model.isEmpty() ? nullptr : model.constData();
    names.layout = layout.isEmpty() ? nullptr : layout.constData();
    names.variant = variant.isEmpty() ? nullptr : variant.constData();
    names.options = options.isEmpty() ? nullptr : options.constData();

    xkbKeymap_ = xkb_keymap_new_from_names(xkbContext_, &names,
                                            XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!xkbKeymap_)
      return;
    xkbState_ = xkb_state_new(xkbKeymap_);
    if (!xkbState_)
      return;

    char *map =
        xkb_keymap_get_as_string(xkbKeymap_, XKB_KEYMAP_FORMAT_TEXT_V1);
    if (!map)
      return;
    QByteArray data(map);
    std::free(map);
    data.append('\0');
    keymapSize_ = static_cast<uint32_t>(data.size());
    keymapFd_ = createAnonymousFile(data);
  }

  InputMethodSupport *owner_ = nullptr;
  QWaylandCompositor *compositor_ = nullptr;
  QQuickWindow *window_ = nullptr;
  QWaylandOutput *output_ = nullptr;

  wl_global *textInputManagerGlobal_ = nullptr;
  wl_global *inputMethodManagerGlobal_ = nullptr;
  wl_global *virtualKeyboardManagerGlobal_ = nullptr;
  InputMethodState *inputMethod_ = nullptr;
  wl_client *inputMethodClient_ = nullptr;
  QHash<wl_resource *, PopupState *> popups_;
  QHash<wl_resource *, VirtualKeyboardState *> virtualKeyboards_;
  QHash<wl_resource *, TextInputState *> textInputs_;
  TextInputState *activeTextInput_ = nullptr;
  bool textActive_ = false;
  bool seatConnected_ = false;

  xkb_context *xkbContext_ = nullptr;
  xkb_keymap *xkbKeymap_ = nullptr;
  xkb_state *xkbState_ = nullptr;
  int keymapFd_ = -1;
  uint32_t keymapSize_ = 0;

  uint32_t lastDepressed_ = UINT32_MAX;
  uint32_t lastLatched_ = UINT32_MAX;
  uint32_t lastLocked_ = UINT32_MAX;
  uint32_t lastGroup_ = UINT32_MAX;

  static const struct zwp_text_input_manager_v3_interface textInputManagerImpl_;
  static const struct zwp_text_input_v3_interface textInputImpl_;
  static const struct zwp_input_method_manager_v2_interface inputMethodManagerImpl_;
  static const struct zwp_input_method_v2_interface inputMethodImpl_;
  static const struct zwp_input_popup_surface_v2_interface inputPopupImpl_;
  static const struct zwp_input_method_keyboard_grab_v2_interface keyboardGrabImpl_;
  static const struct zwp_virtual_keyboard_manager_v1_interface
      virtualKeyboardManagerImpl_;
  static const struct zwp_virtual_keyboard_v1_interface virtualKeyboardImpl_;
};

const struct zwp_text_input_manager_v3_interface
    InputMethodSupport::Impl::textInputManagerImpl_ = {
        InputMethodSupport::Impl::destroyTextInputManager,
        InputMethodSupport::Impl::getTextInput,
};

const struct zwp_text_input_v3_interface
    InputMethodSupport::Impl::textInputImpl_ = {
        InputMethodSupport::Impl::textInputDestroy,
        InputMethodSupport::Impl::textInputEnable,
        InputMethodSupport::Impl::textInputDisable,
        InputMethodSupport::Impl::textInputSetSurrounding,
        InputMethodSupport::Impl::textInputSetChangeCause,
        InputMethodSupport::Impl::textInputSetContentType,
        InputMethodSupport::Impl::textInputSetCursorRectangle,
        InputMethodSupport::Impl::textInputCommit,
        InputMethodSupport::Impl::textInputSetAvailableActions,
        InputMethodSupport::Impl::textInputShowPanel,
        InputMethodSupport::Impl::textInputHidePanel,
};

const struct zwp_input_method_manager_v2_interface
    InputMethodSupport::Impl::inputMethodManagerImpl_ = {
        InputMethodSupport::Impl::getInputMethod,
        InputMethodSupport::Impl::destroyInputMethodManager,
};

const struct zwp_input_method_v2_interface InputMethodSupport::Impl::inputMethodImpl_ = {
    InputMethodSupport::Impl::inputMethodCommitString,
    InputMethodSupport::Impl::inputMethodSetPreedit,
    InputMethodSupport::Impl::inputMethodDeleteSurrounding,
    InputMethodSupport::Impl::inputMethodCommit,
    InputMethodSupport::Impl::inputMethodGetPopup,
    InputMethodSupport::Impl::inputMethodGrabKeyboard,
    InputMethodSupport::Impl::inputMethodDestroy,
};

const struct zwp_input_popup_surface_v2_interface
    InputMethodSupport::Impl::inputPopupImpl_ = {
        InputMethodSupport::Impl::popupDestroy,
};

const struct zwp_input_method_keyboard_grab_v2_interface
    InputMethodSupport::Impl::keyboardGrabImpl_ = {
        InputMethodSupport::Impl::keyboardGrabRelease,
};

const struct zwp_virtual_keyboard_manager_v1_interface
    InputMethodSupport::Impl::virtualKeyboardManagerImpl_ = {
        InputMethodSupport::Impl::createVirtualKeyboard,
};

const struct zwp_virtual_keyboard_v1_interface
    InputMethodSupport::Impl::virtualKeyboardImpl_ = {
        InputMethodSupport::Impl::virtualKeyboardKeymap,
        InputMethodSupport::Impl::virtualKeyboardKey,
        InputMethodSupport::Impl::virtualKeyboardModifiers,
        InputMethodSupport::Impl::virtualKeyboardDestroy,
};

InputMethodSupport::InputMethodSupport(QWaylandCompositor *compositor,
                                       QQuickWindow *window,
                                       QWaylandOutput *output)
    : QObject(compositor),
      d(std::make_unique<Impl>(this, compositor, window, output)) {}

InputMethodSupport::~InputMethodSupport() = default;

bool InputMethodSupport::filterKeyEvent(QKeyEvent *event) {
  return d->filterKeyEvent(event);
}

bool InputMethodSupport::hasKeyboardGrab() const {
  return d->hasKeyboardGrab();
}

bool InputMethodSupport::nativeBridgeAvailable() const {
  return d->nativeBridgeAvailable();
}

InputMethodSupport *installInputMethodProtocols(QWaylandCompositor *compositor,
                                                QQuickWindow *window,
                                                QWaylandOutput *output) {
  return new InputMethodSupport(compositor, window, output);
}

} // namespace LuDash
