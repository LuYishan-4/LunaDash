#include <LuDash/input_settings/InputSettings.h>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/QWaylandSeat>

#ifdef LUDASH_USE_LIBINPUT
#include <QSocketNotifier>
#include <libinput.h>
#include <libudev.h>
#include <xkbcommon/xkbcommon.h>
#include <fcntl.h>
#include <memory>
#include <unistd.h>
#endif

namespace LuDash {
#ifdef LUDASH_USE_LIBINPUT
namespace {
int openRestricted(const char *path, int flags, void *) {
  return ::open(path, flags | O_CLOEXEC);
}
void closeRestricted(int fd, void *) { ::close(fd); }
const libinput_interface kLibinputInterface = {openRestricted, closeRestricted};

class NativeKeyboardState final : public QObject {
public:
  explicit NativeKeyboardState(QObject *parent = nullptr) : QObject(parent) {}
  ~NativeKeyboardState() override {
    notifier_.reset();
    if (input_)
      libinput_unref(input_);
    if (udev_)
      udev_unref(udev_);
    if (state_)
      xkb_state_unref(state_);
    if (keymap_)
      xkb_keymap_unref(keymap_);
    if (context_)
      xkb_context_unref(context_);
  }

  void configure(const QString &layout) {
    if (!context_)
      context_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!context_)
      return;
    if (state_) {
      xkb_state_unref(state_);
      state_ = nullptr;
    }
    if (keymap_) {
      xkb_keymap_unref(keymap_);
      keymap_ = nullptr;
    }
    const QByteArray layoutName = layout.isEmpty() ? QByteArray("us") : layout.toUtf8();
    const xkb_rule_names names = {"evdev", "pc105", layoutName.constData(), nullptr,
                                  nullptr};
    keymap_ = xkb_keymap_new_from_names(context_, &names,
                                        XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (keymap_)
      state_ = xkb_state_new(keymap_);
    ensureInput();
    syncLeds();
  }

private:
  void ensureInput() {
    if (input_)
      return;
    udev_ = udev_new();
    if (!udev_)
      return;
    input_ = libinput_udev_create_context(&kLibinputInterface, nullptr, udev_);
    if (!input_ || libinput_udev_assign_seat(input_, "seat0") != 0) {
      if (input_) {
        libinput_unref(input_);
        input_ = nullptr;
      }
      return;
    }
    notifier_ = std::make_unique<QSocketNotifier>(libinput_get_fd(input_),
                                                   QSocketNotifier::Read, this);
    connect(notifier_.get(), &QSocketNotifier::activated, this,
            [this] { dispatch(); });
    dispatch();
  }

  void dispatch() {
    if (!input_ || libinput_dispatch(input_) != 0)
      return;
    while (libinput_event *event = libinput_get_event(input_)) {
      const auto type = libinput_event_get_type(event);
      if (type == LIBINPUT_EVENT_DEVICE_ADDED) {
        auto *device = libinput_event_get_device(event);
        if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
          libinput_device_ref(device);
          keyboards_.push_back(device);
        }
      } else if (type == LIBINPUT_EVENT_DEVICE_REMOVED) {
        auto *device = libinput_event_get_device(event);
        const auto it = std::find(keyboards_.begin(), keyboards_.end(), device);
        if (it != keyboards_.end()) {
          libinput_device_unref(*it);
          keyboards_.erase(it);
        }
      } else if (type == LIBINPUT_EVENT_KEYBOARD_KEY && state_) {
        auto *keyboard = libinput_event_get_keyboard_event(event);
        const uint32_t key = libinput_event_keyboard_get_key(keyboard) + 8;
        const auto keyState = libinput_event_keyboard_get_key_state(keyboard);
        xkb_state_update_key(state_, key,
                             keyState == LIBINPUT_KEY_STATE_PRESSED
                                 ? XKB_KEY_DOWN
                                 : XKB_KEY_UP);
        syncLeds();
      }
      libinput_event_destroy(event);
      libinput_dispatch(input_);
    }
  }

  void syncLeds() {
    if (!state_)
      return;
    libinput_led leds = static_cast<libinput_led>(0);
    if (xkb_state_led_name_is_active(state_, XKB_LED_NAME_CAPS) > 0)
      leds = static_cast<libinput_led>(leds | LIBINPUT_LED_CAPS_LOCK);
    if (xkb_state_led_name_is_active(state_, XKB_LED_NAME_NUM) > 0)
      leds = static_cast<libinput_led>(leds | LIBINPUT_LED_NUM_LOCK);
    if (xkb_state_led_name_is_active(state_, XKB_LED_NAME_SCROLL) > 0)
      leds = static_cast<libinput_led>(leds | LIBINPUT_LED_SCROLL_LOCK);
    for (auto *device : keyboards_)
      libinput_device_led_update(device, leds);
  }

  udev *udev_ = nullptr;
  libinput *input_ = nullptr;
  xkb_context *context_ = nullptr;
  xkb_keymap *keymap_ = nullptr;
  xkb_state *state_ = nullptr;
  std::unique_ptr<QSocketNotifier> notifier_;
  std::vector<libinput_device *> keyboards_;
};

NativeKeyboardState &nativeKeyboardState() {
  static NativeKeyboardState state;
  return state;
}
} // namespace
#endif

void applyKeyboardPreferences(QWaylandSeat *seat,
                              const QJsonObject &preferences) {
  if (!seat || !seat->keyboard())
    return;
  auto *keymap = seat->keymap();
  keymap->setRules(QStringLiteral("evdev"));
  keymap->setModel(QStringLiteral("pc105"));
  const QString layout = preferences.value("keyboardLayout").toString();
  keymap->setLayout(layout);
  seat->keyboard()->setRepeatRate(
      static_cast<quint32>(preferences.value("keyRepeatRate").toInt()));
  seat->keyboard()->setRepeatDelay(
      static_cast<quint32>(preferences.value("keyRepeatDelay").toInt()));
#ifdef LUDASH_USE_LIBINPUT
  // Qt continues forwarding key events to Wayland clients, while this native
  // state tracker owns the physical lock state and LEDs. Keeping protocol
  // forwarding in one place avoids duplicate key events from two libinput
  // consumers and leaves pointer/touch handling with the active QPA backend.
  nativeKeyboardState().configure(layout);
#endif
}
} // namespace LuDash
