#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {

// The seat's selection: wl_data_device for focused clients and
// wlr-data-control for clipboard managers.
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
