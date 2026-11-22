#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {

// The input-method bridge: one input method (Fcitx, IBus …), the text inputs
// of every client, and keyboard focus changes that move text input between
// surfaces.
class WaylandCompositor::Impl::TextInputComponent final
    : public Component,
      public LunaClint<TextInputComponent> {
public:
  using Component::Component;
  bool start() override;

private:
  void onNewInputMethod(wlr_input_method_v2 *method);
  void onNewTextInput(wlr_text_input_v3 *text);
  void onKeyboardFocusChange(wlr_seat_keyboard_focus_change_event *event);

  Slot newInputMethod;
  Slot newTextInput;
  Slot keyboardFocusChange;
};

} // namespace LunaDash
