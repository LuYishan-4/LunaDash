#include "compositor/wayland/IdleNotify.hpp"

#include <cstddef>
#include <cstdlib>

#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

namespace LunaDash {

struct IdleNotify::Listeners {
  wl_listener newInhibitor{};
  wl_listener inhibitDestroy{};
  IdleNotify *owner = nullptr;

  static Listeners *from(wl_listener *listener, std::size_t offset) {
    return reinterpret_cast<Listeners *>(reinterpret_cast<char *>(listener) - offset);
  }
  static void onNewInhibitor(wl_listener *listener, void *data) {
    from(listener, offsetof(Listeners, newInhibitor))->owner->inhibitorAdded(data);
  }
  static void onInhibitDestroy(wl_listener *listener, void *) {
    auto *self = from(listener, offsetof(Listeners, inhibitDestroy));
    wl_list_remove(&self->newInhibitor.link);
    wl_list_remove(&self->inhibitDestroy.link);
    wl_list_init(&self->newInhibitor.link);
    wl_list_init(&self->inhibitDestroy.link);
  }
};

namespace {

struct InhibitorWatch {
  wl_listener destroy{};
  IdleNotify *owner = nullptr;
  void (*removed)(IdleNotify *) = nullptr;

  static void onDestroy(wl_listener *listener, void *) {
    auto *self = reinterpret_cast<InhibitorWatch *>(
        reinterpret_cast<char *>(listener) - offsetof(InhibitorWatch, destroy));
    wl_list_remove(&self->destroy.link);
    self->removed(self->owner);
    delete self;
  }
};

} // namespace

IdleNotify::IdleNotify(wl_display *display, wlr_idle_inhibit_manager_v1 *inhibit,
                       wlr_seat *seat)
    : seat_(seat) {
  notifier_ = wlr_idle_notifier_v1_create(display);
  if (!notifier_ || !inhibit)
    return;
  listeners_ = new Listeners();
  listeners_->owner = this;
  listeners_->newInhibitor.notify = &Listeners::onNewInhibitor;
  listeners_->inhibitDestroy.notify = &Listeners::onInhibitDestroy;
  wl_signal_add(&inhibit->events.new_inhibitor, &listeners_->newInhibitor);
  wl_signal_add(&inhibit->events.destroy, &listeners_->inhibitDestroy);
}

IdleNotify::~IdleNotify() {
  if (!listeners_)
    return;
  wl_list_remove(&listeners_->newInhibitor.link);
  wl_list_remove(&listeners_->inhibitDestroy.link);
  delete listeners_;
}

void IdleNotify::activity() {
  if (notifier_ && seat_)
    wlr_idle_notifier_v1_notify_activity(notifier_, seat_);
}

void IdleNotify::inhibitorAdded(void *data) {
  auto *inhibitor = static_cast<wlr_idle_inhibitor_v1 *>(data);
  if (!inhibitor)
    return;
  auto *watch = new InhibitorWatch();
  watch->owner = this;
  watch->removed = [](IdleNotify *owner) { owner->inhibitorRemoved(); };
  watch->destroy.notify = &InhibitorWatch::onDestroy;
  wl_signal_add(&inhibitor->events.destroy, &watch->destroy);
  if (++inhibitors_ == 1 && notifier_)
    wlr_idle_notifier_v1_set_inhibited(notifier_, true);
}

void IdleNotify::inhibitorRemoved() {
  if (inhibitors_ > 0 && --inhibitors_ == 0 && notifier_)
    wlr_idle_notifier_v1_set_inhibited(notifier_, false);
}

} // namespace LunaDash
