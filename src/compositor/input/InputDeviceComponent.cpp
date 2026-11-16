#include "compositor/input/InputDeviceComponent.hpp"

namespace LunaDash {

bool WaylandCompositor::Impl::InputDeviceComponent::start() {
  impl->virtualKeyboardManager =
      wlr_virtual_keyboard_manager_v1_create(impl->display);
  if (!impl->virtualKeyboardManager)
    return impl->fail("wlroots could not create the virtual-keyboard global.");
  listen<&InputDeviceComponent::onNewInput>(&impl->backend->events.new_input,
                                            newInput);
  listen<&InputDeviceComponent::onNewVirtualKeyboard>(
      &impl->virtualKeyboardManager->events.new_virtual_keyboard,
      newVirtualKeyboard);
  return true;
}

void WaylandCompositor::Impl::InputDeviceComponent::onNewInput(
    wlr_input_device *device) {
  if (!device)
    return;
  switch (device->type) {
  case WLR_INPUT_DEVICE_KEYBOARD:
    impl->addKeyboard(wlr_keyboard_from_input_device(device), false);
    break;
  case WLR_INPUT_DEVICE_POINTER:
    ++impl->pointerDevices;
    wlr_cursor_attach_input_device(impl->cursor, device);
    impl->updateSeatCapabilities();
    break;
  default:
    break;
  }
}

void WaylandCompositor::Impl::InputDeviceComponent::onNewVirtualKeyboard(
    wlr_virtual_keyboard_v1 *keyboard) {
  if (keyboard)
    impl->addKeyboard(&keyboard->keyboard, true,
                      keyboard->resource
                          ? wl_resource_get_client(keyboard->resource)
                          : nullptr);
}

} // namespace LunaDash
