#include "compositor/wayland/IdleNotify.hpp"

#include <cstdlib>
#include <functional>

#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

namespace LunaDash {

struct IdleNotify::Listeners {
  wl_listener newInhibitor{};
  wl_listener inhibitDestroy{};
  std::function<void(void *)> newInhibitorCallback;

  static void onNewInhibitor(wl_listener *listener, void *data) {
    Listeners *self = wl_container_of(listener, self, newInhibitor);
    self->newInhibitorCallback(data);
  }
  static void onInhibitDestroy(wl_listener *listener, void *) {
    Listeners *self = wl_container_of(listener, self, inhibitDestroy);
    // wlr_idle_inhibit_manager_v1 init list :47
    wl_list_remove(&self->newInhibitor.link);
    wl_list_remove(&self->inhibitDestroy.link);
    wl_list_init(&self->newInhibitor.link);
    wl_list_init(&self->inhibitDestroy.link);
  }
};

namespace {
struct InhibitorWatch {
  wl_listener destroy{};
  std::function<void()> removed;

  static void onDestroy(wl_listener *listener, void *) {
    InhibitorWatch *self = wl_container_of(listener, self, destroy);
    wl_list_remove(&self->destroy.link);
    self->removed();
    delete self;
  }
};

} // namespace

IdleNotify::IdleNotify(wl_display *display,
                       wlr_idle_inhibit_manager_v1 *inhibit, wlr_seat *seat)
    : seat_(seat) {
  notifier_ = wlr_idle_notifier_v1_create(display);
  if (!notifier_ || !inhibit)
    return;
  listeners_ = new Listeners();
  listeners_->newInhibitorCallback = [this](void *data) {
    inhibitorAdded(data);
  };
  listeners_->newInhibitor.notify = &Listeners::onNewInhibitor;
  listeners_->inhibitDestroy.notify = &Listeners::onInhibitDestroy;
  // wlroots initializes these signal heads when creating the manager.
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
  watch->removed = [this] { inhibitorRemoved(); };
  watch->destroy.notify = &InhibitorWatch::onDestroy;
  wl_signal_add(&inhibitor->events.destroy, &watch->destroy);

  //stop idle
  if (++inhibitors_ == 1 && notifier_)
    wlr_idle_notifier_v1_set_inhibited(notifier_, true);
}

void IdleNotify::inhibitorRemoved() {
  if (inhibitors_ > 0 && --inhibitors_ == 0 && notifier_)
    wlr_idle_notifier_v1_set_inhibited(notifier_, false);
}

} // namespace LunaDash
