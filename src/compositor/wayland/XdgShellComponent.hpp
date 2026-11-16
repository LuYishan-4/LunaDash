#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {

// xdg-shell windows and popups as clients create them, xdg-activation focus
// requests, and the wlr foreign-toplevel-management compatibility global.
class WaylandCompositor::Impl::XdgShellComponent final
    : public Component,
      public LunaClint<XdgShellComponent> {
public:
  using Component::Component;
  bool start() override;

private:
#if WLR_VERSION_MINOR < 18
  void onNewSurface(wlr_xdg_surface *surface);
#else
  void onNewToplevel(wlr_xdg_toplevel *toplevel);
  void onNewPopup(wlr_xdg_popup *popup);
#endif
#if LUDASH_WLR_HAS_XDG_ACTIVATION
  void onActivationRequest(wlr_xdg_activation_v1_request_activate_event *request);
#endif

  Slot<XdgShellComponent> newSurface;
  Slot<XdgShellComponent> newPopup;
  Slot<XdgShellComponent> activationRequest;
};

} // namespace LunaDash
