#include "desktop/InputSettings/InputSettings.hpp"
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/QWaylandSeat>

#ifdef LUDASH_USE_LIBINPUT
#include <QSocketNotifier>
#include <QTimer>
#include <algorithm>
#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <memory>
#include <unistd.h>
#include <vector>
#include <xkbcommon/xkbcommon.h>
#endif

namespace LuDash {
namespace {
bool nativeKeyboardInputEnabled = true;
QString xkbLayoutForPreference(const QString &layout) {
  // Taiwan PC keyboards use the normal ANSI/US physical key positions for
  // Latin text and for Fcitx Zhuyin/Chewing key input. xkeyboard-config's
  // "tw" symbol map is not a Chinese input method and changes the base keymap,
  // which makes Fcitx keystrokes appear incorrect. Keep "tw" as the desktop
  // preference while feeding a US physical map to the Wayland seat; Fcitx is
  // responsible for Traditional Chinese composition.
  return layout == QStringLiteral("tw") ? QStringLiteral("us") : layout;
}
}
#ifdef LUDASH_USE_LIBINPUT
namespace {
int openRestricted(const char *path, int flags, void *) { return ::open(path, flags | O_CLOEXEC); }
void closeRestricted(int fd, void *) { ::close(fd); }
const libinput_interface kLibinputInterface = {openRestricted, closeRestricted};
class NativeKeyboardState final : public QObject {
public:
  explicit NativeKeyboardState(QObject *parent = nullptr) : QObject(parent) {}
  ~NativeKeyboardState() override { stop(); if (state_) xkb_state_unref(state_); if (keymap_) xkb_keymap_unref(keymap_); if (context_) xkb_context_unref(context_); }
  void stop() { notifier_.reset(); for (auto *device : keyboards_) libinput_device_unref(device); keyboards_.clear(); if (input_) { libinput_unref(input_); input_ = nullptr; } if (udev_) { udev_unref(udev_); udev_ = nullptr; } seat_ = nullptr; dispatchScheduled_ = false; ledStateValid_ = false; }
  void configure(QWaylandSeat *seat, const QString &layout) {
    seat_ = seat;
    if (!context_) context_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS); if (!context_) return;
    if (state_) xkb_state_unref(state_); if (keymap_) xkb_keymap_unref(keymap_); state_ = nullptr; keymap_ = nullptr;
    const QByteArray layoutName = layout.isEmpty() ? QByteArray("us") : layout.toUtf8();
    const xkb_rule_names names = {"evdev", "pc105", layoutName.constData(), nullptr, nullptr};
    keymap_ = xkb_keymap_new_from_names(context_, &names, XKB_KEYMAP_COMPILE_NO_FLAGS); if (keymap_) state_ = xkb_state_new(keymap_);
    ensureInput(); syncLeds(true);
  }
private:
  static constexpr int kMaxEventsPerTurn = 32;
  void ensureInput() {
    if (input_) return; udev_ = udev_new(); if (!udev_) return;
    input_ = libinput_udev_create_context(&kLibinputInterface, nullptr, udev_);
    if (!input_ || libinput_udev_assign_seat(input_, "seat0") != 0) { if (input_) libinput_unref(input_); input_ = nullptr; udev_unref(udev_); udev_ = nullptr; return; }
    notifier_ = std::make_unique<QSocketNotifier>(libinput_get_fd(input_), QSocketNotifier::Read, this);
    connect(notifier_.get(), &QSocketNotifier::activated, this, [this] { scheduleDispatch(); }); scheduleDispatch();
  }
  void scheduleDispatch() { if (dispatchScheduled_) return; dispatchScheduled_ = true; QTimer::singleShot(0, this, [this] { dispatchScheduled_ = false; dispatchBatch(); }); }
  void dispatchBatch() {
    if (!input_ || libinput_dispatch(input_) != 0) return; bool ledsDirty = false; int processed = 0;
    while (processed < kMaxEventsPerTurn) {
      libinput_event *event = libinput_get_event(input_); if (!event) break; ++processed; const auto type = libinput_event_get_type(event);
      if (type == LIBINPUT_EVENT_DEVICE_ADDED) { auto *device = libinput_event_get_device(event); if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD) && std::find(keyboards_.begin(), keyboards_.end(), device) == keyboards_.end()) { libinput_device_ref(device); keyboards_.push_back(device); ledsDirty = true; } }
      else if (type == LIBINPUT_EVENT_DEVICE_REMOVED) { auto *device = libinput_event_get_device(event); const auto it = std::find(keyboards_.begin(), keyboards_.end(), device); if (it != keyboards_.end()) { libinput_device_unref(*it); keyboards_.erase(it); } }
      else if (type == LIBINPUT_EVENT_KEYBOARD_KEY && state_) {
        auto *keyboardEvent = libinput_event_get_keyboard_event(event);
        const uint32_t evdevKey = libinput_event_keyboard_get_key(keyboardEvent);
        const uint32_t xkbKey = evdevKey + 8;
        const bool pressed = libinput_event_keyboard_get_key_state(keyboardEvent) == LIBINPUT_KEY_STATE_PRESSED;
        xkb_state_update_key(state_, xkbKey, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
        if (seat_ && seat_->keyboard()) {
          // QWaylandKeyboard's public sendKey*Event API takes the native/XKB
          // scan code domain. Qt converts it back to wl_keyboard's evdev code
          // internally by subtracting the historical offset of 8. Passing the
          // raw evdev code here caused exactly that subtraction twice: e.g.
          // S (31) became I (23), D (32) became O (24).
          if (pressed) seat_->keyboard()->sendKeyPressEvent(xkbKey);
          else seat_->keyboard()->sendKeyReleaseEvent(xkbKey);
        }
        ledsDirty = true;
      }
      libinput_event_destroy(event);
    }
    if (ledsDirty) syncLeds(false); if (processed == kMaxEventsPerTurn) scheduleDispatch();
  }
  void syncLeds(bool force) {
    if (!state_) return; libinput_led leds = static_cast<libinput_led>(0);
    if (xkb_state_led_name_is_active(state_, XKB_LED_NAME_CAPS) > 0) leds = static_cast<libinput_led>(leds | LIBINPUT_LED_CAPS_LOCK);
    if (xkb_state_led_name_is_active(state_, XKB_LED_NAME_NUM) > 0) leds = static_cast<libinput_led>(leds | LIBINPUT_LED_NUM_LOCK);
    if (xkb_state_led_name_is_active(state_, XKB_LED_NAME_SCROLL) > 0) leds = static_cast<libinput_led>(leds | LIBINPUT_LED_SCROLL_LOCK);
    if (!force && ledStateValid_ && leds == lastLeds_) return; lastLeds_ = leds; ledStateValid_ = true; for (auto *device : keyboards_) libinput_device_led_update(device, leds);
  }
  QWaylandSeat *seat_ = nullptr; udev *udev_ = nullptr; libinput *input_ = nullptr; xkb_context *context_ = nullptr; xkb_keymap *keymap_ = nullptr; xkb_state *state_ = nullptr;
  std::unique_ptr<QSocketNotifier> notifier_; std::vector<libinput_device *> keyboards_; libinput_led lastLeds_ = static_cast<libinput_led>(0); bool ledStateValid_ = false; bool dispatchScheduled_ = false;
};
NativeKeyboardState &nativeKeyboardState() { static NativeKeyboardState state; return state; }
}
#endif

void setNativeKeyboardInputEnabled(bool enabled) {
  nativeKeyboardInputEnabled = enabled;
#ifdef LUDASH_USE_LIBINPUT
  if (!enabled) nativeKeyboardState().stop();
#else
  Q_UNUSED(enabled);
#endif
}
void applyKeyboardPreferences(QWaylandSeat *seat, const QJsonObject &preferences) {
  if (!seat || !seat->keyboard()) return;
  auto *keymap = seat->keymap();
  keymap->setRules(QStringLiteral("evdev"));
  keymap->setModel(QStringLiteral("pc105"));
  const QString preferenceLayout = preferences.value("keyboardLayout").toString(QStringLiteral("us"));
  const QString xkbLayout = xkbLayoutForPreference(preferenceLayout);
  keymap->setLayout(xkbLayout);
  constexpr quint32 kRepeatRate = 25;
  constexpr quint32 kRepeatDelay = 600;
  seat->keyboard()->setRepeatRate(kRepeatRate);
  seat->keyboard()->setRepeatDelay(kRepeatDelay);
#ifdef LUDASH_USE_LIBINPUT
  if (nativeKeyboardInputEnabled) nativeKeyboardState().configure(seat, xkbLayout);
#endif
}
} // namespace LuDash
