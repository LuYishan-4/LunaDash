#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {


class WaylandCompositor::Impl::OutputComponent final
    : public Component,
      public LunaClint<OutputComponent> {
public:
  using Component::Component;
  bool start() override;

private:
  void onNewOutput(wlr_output *output);
  bool commitInitialState(wlr_output *output);

  Slot newOutput;
};

} // namespace LunaDash
