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
#endif

namespace LuDash {
namespace {
QString xkbLayoutForPreference(const QString &layout) {
  // Taiwan PC keyboards normally use US physical key positions. Traditional
  // Chinese composition is handled by Fcitx rather than by the XKB "tw" map.
  return layout == QStringLiteral("tw") ? QStringLiteral("us") : layout;
}
} // namespace

#ifdef LUDASH_USE_LIBINPUT
namespace {
int openRestricted(const char *path, int flags, void *) {
  return ::open(path, flags | O_CLOEXEC);
}

void closeRestricted(int fd, void *) { ::close(fd); }

const libinput_interface kLibinputInterface = {openRestricted, closeRestricted};

class KeyboardLedController final : public QObject {
public:
  explicit KeyboardLedController(QObject *parent = nullptr) : QObject(parent) {}
  ~KeyboardLedController() override { stop(); }

  void setEnabled(bool enabled) {
    if (enabled_ == enabled)
      return;
    enabled_ = enabled;
    if (enabled_)
      ensureInput();
    else
      stop();
  }

  void toggleLock(int qtKey) {
    if (!enabled_)
      return;
    if (qtKey == Qt::Key_CapsLock)
      caps_ = !caps_;
    else if (qtKey == Qt::Key_NumLock)
      num_ = !num_;
    else if (qtKey == Qt::Key_ScrollLock)
      scroll_ = !scroll_;
    else
      return;
    syncLeds(true);
  }

private:
  static constexpr int kMaxEventsPerTurn = 64;

  void stop() {
    notifier_.reset();
    for (auto *device : keyboards_)
      libinput_device_unref(device);
    keyboards_.clear();
    if (input_) {
      libinput_unref(input_);
      input_ = nullptr;
    }
    if (udev_) {
      udev_unref(udev_);
      udev_ = nullptr;
    }
    dispatchScheduled_ = false;
    ledStateValid_ = false;
  }

  void ensureInput() {
    if (!enabled_ || input_)
      return;
    udev_ = udev_new();
    if (!udev_)
      return;
    input_ = libinput_udev_create_context(&kLibinputInterface, nullptr, udev_);
    if (!input_ || libinput_udev_assign_seat(input_, "seat0") != 0) {
      if (input_)
        libinput_unref(input_);
      input_ = nullptr;
      udev_unref(udev_);
      udev_ = nullptr;
      return;
    }

    notifier_ = std::make_unique<QSocketNotifier>(
        libinput_get_fd(input_), QSocketNotifier::Read, this);
    connect(notifier_.get(), &QSocketNotifier::activated, this,
            [this] { scheduleDispatch(); });
    scheduleDispatch();
  }

  void scheduleDispatch() {
    if (!enabled_ || dispatchScheduled_)
      return;
    dispatchScheduled_ = true;
    QTimer::singleShot(0, this, [this] {
      dispatchScheduled_ = false;
      dispatchBatch();
    });
  }

  void dispatchBatch() {
    if (!input_ || libinput_dispatch(input_) != 0)
      return;

    bool devicesChanged = false;
    int processed = 0;
    while (processed < kMaxEventsPerTurn) {
      libinput_event *event = libinput_get_event(input_);
      if (!event)
        break;
      ++processed;

      const auto type = libinput_event_get_type(event);
      if (type == LIBINPUT_EVENT_DEVICE_ADDED) {
        auto *device = libinput_event_get_device(event);
        if (libinput_device_has_capability(device,
                                           LIBINPUT_DEVICE_CAP_KEYBOARD) &&
            std::find(keyboards_.begin(), keyboards_.end(), device) ==
                keyboards_.end()) {
          libinput_device_ref(device);
          keyboards_.push_back(device);
          devicesChanged = true;
        }
      } else if (type == LIBINPUT_EVENT_DEVICE_REMOVED) {
        auto *device = libinput_event_get_device(event);
        const auto it =
            std::find(keyboards_.begin(), keyboards_.end(), device);
        if (it != keyboards_.end()) {
          libinput_device_unref(*it);
          keyboards_.erase(it);
        }
      }
      // Intentionally ignore LIBINPUT_EVENT_KEYBOARD_KEY. Qt QPA owns the
      // keyboard input path; this second libinput context exists only so the
      // compositor can drive the physical lock LEDs without sending a second
      // copy of any key to QWaylandSeat.
      libinput_event_destroy(event);
    }

    if (devicesChanged)
      syncLeds(true);
    if (processed == kMaxEventsPerTurn)
      scheduleDispatch();
  }

  void syncLeds(bool force) {
    libinput_led leds = static_cast<libinput_led>(0);
    if (caps_)
      leds = static_cast<libinput_led>(leds | LIBINPUT_LED_CAPS_LOCK);
    if (num_)
      leds = static_cast<libinput_led>(leds | LIBINPUT_LED_NUM_LOCK);
    if (scroll_)
      leds = static_cast<libinput_led>(leds | LIBINPUT_LED_SCROLL_LOCK);

    if (!force && ledStateValid_ && leds == lastLeds_)
      return;
    lastLeds_ = leds;
    ledStateValid_ = true;
    for (auto *device : keyboards_)
      libinput_device_led_update(device, leds);
  }

  bool enabled_ = false;
  bool caps_ = false;
  bool num_ = false;
  bool scroll_ = false;
  bool ledStateValid_ = false;
  bool dispatchScheduled_ = false;
  udev *udev_ = nullptr;
  libinput *input_ = nullptr;
  std::unique_ptr<QSocketNotifier> notifier_;
  std::vector<libinput_device *> keyboards_;
  libinput_led lastLeds_ = static_cast<libinput_led>(0);
};

KeyboardLedController &keyboardLedController() {
  static KeyboardLedController controller;
  return controller;
}
} // namespace
#endif

void setKeyboardLedControlEnabled(bool enabled) {
#ifdef LUDASH_USE_LIBINPUT
  keyboardLedController().setEnabled(enabled);
#else
  Q_UNUSED(enabled);
#endif
}

void handleKeyboardLockKey(int qtKey, bool pressed, bool autoRepeat) {
#ifdef LUDASH_USE_LIBINPUT
  if (pressed && !autoRepeat)
    keyboardLedController().toggleLock(qtKey);
#else
  Q_UNUSED(qtKey);
  Q_UNUSED(pressed);
  Q_UNUSED(autoRepeat);
#endif
}

void applyKeyboardPreferences(QWaylandSeat *seat,
                              const QJsonObject &preferences) {
  if (!seat || !seat->keyboard())
    return;

  auto *keymap = seat->keymap();
  keymap->setRules(QStringLiteral("evdev"));
  keymap->setModel(QStringLiteral("pc105"));

  const QString preferenceLayout =
      preferences.value("keyboardLayout").toString(QStringLiteral("us"));
  keymap->setLayout(xkbLayoutForPreference(preferenceLayout));

  constexpr quint32 kRepeatRate = 25;
  constexpr quint32 kRepeatDelay = 600;
  seat->keyboard()->setRepeatRate(kRepeatRate);
  seat->keyboard()->setRepeatDelay(kRepeatDelay);
}
} // namespace LuDash
