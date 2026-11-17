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
  ~WaylandSlot() {
    if (connected)
      wl_list_remove(&listener.link);
  }
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

template <typename... Slots> void detachListeners(Slots &...slots) {
  (detachListener(slots), ...);
}

template <typename Owner> Owner *listenerOwner(wl_listener *listener) {
  auto *slot = reinterpret_cast<WaylandSlot<Owner> *>(listener);
  return slot ? slot->owner : nullptr;
}

// Splits `void (Owner::*)(Event *)` and `void (Owner::*)()` into their parts.
template <typename Method> struct SlotMethod;

template <typename OwnerType, typename EventType>
struct SlotMethod<void (OwnerType::*)(EventType *)> {
  using Owner = OwnerType;
  using Event = EventType;
  static constexpr bool takesEvent = true;
};

template <typename OwnerType> struct SlotMethod<void (OwnerType::*)()> {
  using Owner = OwnerType;
  using Event = void;
  static constexpr bool takesEvent = false;
};

template <auto Method>
using SlotOwner = typename SlotMethod<decltype(Method)>::Owner;


template <auto Method>
void connect(wl_signal *signal, WaylandSlot<SlotOwner<Method>> &slot,
             SlotOwner<Method> *owner) {
  using Traits = SlotMethod<decltype(Method)>;
  attachListener(signal, slot, owner, [](wl_listener *listener, void *data) {
    auto *self = listenerOwner<typename Traits::Owner>(listener);
    if (!self)
      return;
    if constexpr (Traits::takesEvent)
      (self->*Method)(static_cast<typename Traits::Event *>(data));
    else
      (self->*Method)();
  });
}

template <auto Method>
void connectFirst(wl_signal *signal, WaylandSlot<SlotOwner<Method>> &slot,
                  SlotOwner<Method> *owner) {
  connect<Method>(signal, slot, owner);
  if (!slot.connected)
    return;
  wl_list_remove(&slot.listener.link);
  wl_list_insert(&signal->listener_list, &slot.listener.link);
}

template <auto Method>
wl_event_source *addTimer(wl_event_loop *loop, SlotOwner<Method> *owner) {
  static_assert(!SlotMethod<decltype(Method)>::takesEvent,
                "timer handlers take no arguments");
  if (!loop)
    return nullptr;
  return wl_event_loop_add_timer(
      loop,
      [](void *data) {
        (static_cast<SlotOwner<Method> *>(data)->*Method)();
        return 0;
      },
      owner);
}

} // namespace LunaDash::Templates
