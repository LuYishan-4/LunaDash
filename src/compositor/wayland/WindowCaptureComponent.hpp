#pragma once

#include "compositor/wayland/Register.hpp"

namespace LunaDash {

// ext-image-capture globals: the toplevel list portals pick windows from, and
// the scene-backed capture sources for outputs and single windows.
class WaylandCompositor::Impl::WindowCaptureComponent final
    : public Component,
      public LunaClint<WindowCaptureComponent> {
public:
  using Component::Component;
  bool start() override;

private:
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  void onCaptureRequest(
      wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request);
#endif

  Slot<WindowCaptureComponent> captureRequest;
};

} // namespace LunaDash
