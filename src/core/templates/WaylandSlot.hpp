#pragma once

extern "C" {
#include <wayland-server-core.h>
}

#include <cstddef>
#include <type_traits>

namespace LunaDash::Templates {

struct WaylandSlot {
  wl_listener listener{};
  bool connected = false;

  WaylandSlot() = default;
  WaylandSlot(const WaylandSlot &) = delete;
  WaylandSlot &operator=(const WaylandSlot &) = delete;
  ~WaylandSlot() {
    if (connected)
      wl_list_remove(&listener.link);
  }
};

inline void attachListener(wl_signal *signal, WaylandSlot &slot,
                           wl_notify_func_t notify) {
  if (!signal || slot.connected)
    return;
  slot.listener.notify = notify;
  wl_signal_add(signal, &slot.listener);
  slot.connected = true;
}

inline void detachListener(WaylandSlot &slot) {
  if (!slot.connected)
    return;
  wl_list_remove(&slot.listener.link);
  slot.connected = false;
}

template <typename... Slots> void detachListeners(Slots &...slots) {
  (detachListener(slots), ...);
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

// The class a `WaylandSlot Owner::*` member belongs to.
template <typename Member> struct SlotMember;
template <typename OwnerType> struct SlotMember<WaylandSlot OwnerType::*> {
  using Owner = OwnerType;
};

template <auto Member>
using SlotMemberOwner = typename SlotMember<decltype(Member)>::Owner;

// The owner of the listener of slot `Member`: wl_container_of, with the member
// given as a pointer to member. offsetof cannot take one, nor classes with
// virtual functions, so the offset is measured on storage shaped like an
// Owner; no object lives there and nothing in it is read.
template <auto Member>
SlotMemberOwner<Member> *slotOwner(wl_listener *listener) {
  using Owner = SlotMemberOwner<Member>;
  alignas(Owner) static const unsigned char shape[sizeof(Owner)]{};
  const auto *probe = reinterpret_cast<const Owner *>(shape);
  const std::ptrdiff_t offset =
      reinterpret_cast<const unsigned char *>(&(probe->*Member).listener) -
      shape;
  return reinterpret_cast<Owner *>(reinterpret_cast<unsigned char *>(listener) -
                                   offset);
}

// Connects `Method` of the object holding slot `Member` to `signal`.
template <auto Method, auto Member>
void connect(wl_signal *signal, WaylandSlot &slot) {
  static_assert(std::is_same_v<SlotOwner<Method>, SlotMemberOwner<Member>>,
                "the handler and the slot belong to the same class");
  using Traits = SlotMethod<decltype(Method)>;
  attachListener(signal, slot, [](wl_listener *listener, void *data) {
    auto *self = slotOwner<Member>(listener);
    if constexpr (Traits::takesEvent)
      (self->*Method)(static_cast<typename Traits::Event *>(data));
    else
      (self->*Method)();
  });
}

// connect(), but notified before the listeners already there.
template <auto Method, auto Member>
void connectFirst(wl_signal *signal, WaylandSlot &slot) {
  connect<Method, Member>(signal, slot);
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
