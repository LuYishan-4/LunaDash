#include <LuDash/input_settings/InputSettings.h>
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
namespace { bool nativeKeyboardInputEnabled = true; }
#ifdef LUDASH_USE_LIBINPUT
namespace {
int openRestricted(const char *path, int flags, void *) { return ::open(path, flags | O_CLOEXEC); }
void closeRestricted(int fd, void *) { ::close(fd); }
const libinput_interface kLibinputInterface = {openRestricted, closeRestricted};
class NativeKeyboardState final : public QObject {
public:
  explicit NativeKeyboardState(QObject *parent = nullptr) : QObject(parent) {}
  ~NativeKeyboardState() override { stop(); if (state_) xkb_state_unref(state_); if (keymap_) xkb_keymap_unref(keymap_); if (context_) xkb_context_unref(context_); }
  void stop() { notifier_.reset(); for (auto *device : keyboards_) libinput_device_unref(device); keyboards_.clear(); if (input_) { libinput_unref(input_); input_ = nullptr; } if (udev_) { udev_unref(udev_); udev_ = nullptr; } dispatchScheduled_ = false; ledStateValid_ = false; }
  void configure(const QString &layout) {
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
      else if (type == LIBINPUT_EVENT_KEYBOARD_KEY && state_) { auto *keyboard = libinput_event_get_keyboard_event(event); const uint32_t key = libinput_event_keyboard_get_key(keyboard) + 8; xkb_state_update_key(state_, key, libinput_event_keyboard_get_key_state(keyboard) == LIBINPUT_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP); ledsDirty = true; }
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
  udev *udev_ = nullptr; libinput *input_ = nullptr; xkb_context *context_ = nullptr; xkb_keymap *keymap_ = nullptr; xkb_state *state_ = nullptr;
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
  if (!seat || !seat->keyboard()) return; auto *keymap = seat->keymap(); keymap->setRules(QStringLiteral("evdev")); keymap->setModel(QStringLiteral("pc105"));
  const QString layout = preferences.value("keyboardLayout").toString(); keymap->setLayout(layout);
  seat->keyboard()->setRepeatRate(static_cast<quint32>(preferences.value("keyRepeatRate").toInt())); seat->keyboard()->setRepeatDelay(static_cast<quint32>(preferences.value("keyRepeatDelay").toInt()));
#ifdef LUDASH_USE_LIBINPUT
  if (nativeKeyboardInputEnabled) nativeKeyboardState().configure(layout);
#endif
}
} // namespace LuDash
