#pragma once

#include "core/templates/WaylandSlot.hpp"

#include <type_traits>

namespace LunaDash::Templates {

template <typename Derived> class LunaClint {
protected:
  // Connects handler `Method` to `signal` through this object's slot `Member`:
  // listen<&Self::onCommit, &Self::commit>(&surface->events.commit).
  template <auto Method, auto Member> void listen(wl_signal *signal) {
    static_assert(std::is_same_v<SlotMemberOwner<Member>, Derived>,
                  "listen<> takes a slot of the listening type");
    connect<Method, Member>(signal, self()->*Member);
  }

  template <auto Method, auto Member> void listenFirst(wl_signal *signal) {
    static_assert(std::is_same_v<SlotMemberOwner<Member>, Derived>,
                  "listenFirst<> takes a slot of the listening type");
    connectFirst<Method, Member>(signal, self()->*Member);
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
