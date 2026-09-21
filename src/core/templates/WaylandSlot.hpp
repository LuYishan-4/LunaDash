#pragma once

extern "C" {
#include <wayland-server-core.h>
}

namespace LunaDash::Templates {

template <typename Owner> struct WaylandSlot {
  wl_listener listener{};
  Owner *owner = nullptr;
  bool connected = false;

  WaylandSlot() = default;
  WaylandSlot(const WaylandSlot &) = delete;
  WaylandSlot &operator=(const WaylandSlot &) = delete;
};

template <typename Owner>
void attachListener(wl_signal *signal, WaylandSlot<Owner> &slot, Owner *owner,
                    wl_notify_func_t notify) {
  if (!signal || slot.connected)
    return;
  slot.owner = owner;
  slot.listener.notify = notify;
  wl_signal_add(signal, &slot.listener);
  slot.connected = true;
}

template <typename Owner> void detachListener(WaylandSlot<Owner> &slot) {
  if (!slot.connected)
    return;
  wl_list_remove(&slot.listener.link);
  slot.connected = false;
  slot.owner = nullptr;
}

template <typename Owner> Owner *listenerOwner(wl_listener *listener) {
  auto *slot = reinterpret_cast<WaylandSlot<Owner> *>(listener);
  return slot ? slot->owner : nullptr;
}

} // namespace LunaDash::Templates
