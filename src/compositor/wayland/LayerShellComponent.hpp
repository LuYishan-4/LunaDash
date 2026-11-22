#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {

// wlr-layer-shell surfaces (panel, dock, wallpaper, overlays) as clients create
// them, and the ext-background-effect global their blur requests use.
class WaylandCompositor::Impl::LayerShellComponent final
    : public Component,
      public LunaClint<LayerShellComponent> {
public:
  using Component::Component;
  bool start() override;

private:
  void onNewSurface(wlr_layer_surface_v1 *surface);

  Slot newSurface;
};

} // namespace LunaDash
