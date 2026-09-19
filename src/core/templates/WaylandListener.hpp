#pragma once

extern "C" {
#include <wayland-server-core.h>
}

namespace LunaDash::Templates {

template <typename Owner> struct ListenerSlot {
  wl_listener listener{};
  Owner *owner = nullptr;
  bool connected = false;

  ListenerSlot() = default;
  ListenerSlot(const ListenerSlot &) = delete;
  ListenerSlot &operator=(const ListenerSlot &) = delete;
};

template <typename Owner>
void attachListener(wl_signal *signal, ListenerSlot<Owner> &slot, Owner *owner,
                    wl_notify_func_t notify) {
  if (!signal || slot.connected)
    return;
  slot.owner = owner;
  slot.listener.notify = notify;
  wl_signal_add(signal, &slot.listener);
  slot.connected = true;
}

template <typename Owner> void detachListener(ListenerSlot<Owner> &slot) {
  if (!slot.connected)
    return;
  wl_list_remove(&slot.listener.link);
  slot.connected = false;
  slot.owner = nullptr;
}

template <typename Owner> Owner *listenerOwner(wl_listener *listener) {
  auto *slot = reinterpret_cast<ListenerSlot<Owner> *>(listener);
  return slot ? slot->owner : nullptr;
}

} // namespace LunaDash::Templates
