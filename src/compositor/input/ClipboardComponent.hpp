#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {


class WaylandCompositor::Impl::ClipboardComponent final
    : public Component,
      public LunaClint<ClipboardComponent> {
public:
  using Component::Component;
  bool start() override;

private:
  void onRequestSelection(wlr_seat_request_set_selection_event *event);

  Slot<ClipboardComponent> requestSelection;
};

} // namespace LunaDash
