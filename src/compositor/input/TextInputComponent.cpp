#include "compositor/input/TextInputComponent.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"

namespace LunaDash {

bool WaylandCompositor::Impl::TextInputComponent::start() {
  impl->inputMethodManager = wlr_input_method_manager_v2_create(impl->display);
  impl->textInputManager = wlr_text_input_manager_v3_create(impl->display);
  if (!impl->inputMethodManager || !impl->textInputManager)
    return impl->fail("wlroots could not create the text-input globals.");
  listen<&TextInputComponent::onNewInputMethod,
         &TextInputComponent::newInputMethod>(
      WlrootsCompat::newInputMethodSignal(impl->inputMethodManager));
  listen<&TextInputComponent::onNewTextInput,
         &TextInputComponent::newTextInput>(
      WlrootsCompat::newTextInputSignal(impl->textInputManager));
  listen<&TextInputComponent::onKeyboardFocusChange,
         &TextInputComponent::keyboardFocusChange>(
      &impl->seat->keyboard_state.events.focus_change);
  return true;
}

void WaylandCompositor::Impl::TextInputComponent::onNewInputMethod(
    wlr_input_method_v2 *method) {
  if (!method)
    return;
  if (impl->inputMethod) {
    wlr_input_method_v2_send_unavailable(method);
    return;
  }
  impl->inputMethod = new InputMethodState(impl, method);
  if (impl->activeTextInput)
    impl->syncTextInputToMethod(impl->activeTextInput, true);
}

void WaylandCompositor::Impl::TextInputComponent::onNewTextInput(
    wlr_text_input_v3 *text) {
  if (!text)
    return;
  impl->textInputs.push_back(new TextInputState(impl, text));
  if (auto *focused = impl->seat->keyboard_state.focused_surface)
    impl->updateTextInputFocus(focused);
}

void WaylandCompositor::Impl::TextInputComponent::onKeyboardFocusChange(
    wlr_seat_keyboard_focus_change_event *event) {
  if (event)
    impl->updateTextInputFocus(event->new_surface);
}

} // namespace LunaDash
