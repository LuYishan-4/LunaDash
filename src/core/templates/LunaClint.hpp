#pragma once

#include "core/templates/WaylandSlot.hpp"

#include <type_traits>

namespace LunaDash::Templates {

template <typename Derived> class LunaClint {
protected:
  template <auto Method>
  void listen(wl_signal *signal, WaylandSlot<Derived> &slot) {
    static_assert(std::is_same_v<SlotOwner<Method>, Derived>,
                  "listen<> takes a handler of the listening type");
    connect<Method>(signal, slot, self());
  }

  template <auto Method>
  void listenFirst(wl_signal *signal, WaylandSlot<Derived> &slot) {
    static_assert(std::is_same_v<SlotOwner<Method>, Derived>,
                  "listenFirst<> takes a handler of the listening type");
    connectFirst<Method>(signal, slot, self());
  }

  template <auto Method> wl_event_source *timer(wl_event_loop *loop) {
    static_assert(std::is_same_v<SlotOwner<Method>, Derived>,
                  "timer<> takes a handler of the listening type");
    return addTimer<Method>(loop, self());
  }

private:
  Derived *self() { return static_cast<Derived *>(this); }
};

} // namespace LunaDash::Templates
