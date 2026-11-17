#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {

class WaylandCompositor::Impl::InputDeviceComponent final
    : public Component,
      public LunaClint<InputDeviceComponent> {
public:
  using Component::Component;
  bool start() override;

private:
  void onNewInput(wlr_input_device *device);
  void onNewVirtualKeyboard(wlr_virtual_keyboard_v1 *keyboard);

  Slot<InputDeviceComponent> newInput;
  Slot<InputDeviceComponent> newVirtualKeyboard;
};

} // namespace LunaDash
