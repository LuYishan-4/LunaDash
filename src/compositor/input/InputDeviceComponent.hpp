#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {

// Keyboards and pointers as they are plugged in, and virtual keyboards that
// clients create through virtual-keyboard-v1.
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
