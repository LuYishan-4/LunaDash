#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {

// Outputs as they are plugged in: picks and commits the initial mode, then
// hands each output to an OutputState. Owns the xdg-output and screencopy
// globals.
class WaylandCompositor::Impl::OutputComponent final
    : public Component,
      public LunaClint<OutputComponent> {
public:
  using Component::Component;
  bool start() override;

private:
  void onNewOutput(wlr_output *output);
  // Saved mode and scale when the output accepts them, the preferred mode
  // otherwise.
  bool commitInitialState(wlr_output *output);

  Slot<OutputComponent> newOutput;
};

} // namespace LunaDash
