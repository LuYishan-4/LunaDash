#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/Register.hpp"

namespace LunaDash {


void WaylandCompositor::Impl::applyDecorationMode(ClientWindow *client) {
  auto *decoration = client ? client->decoration.object : nullptr;
  if (!decoration || !decoration->toplevel->base->initialized)
    return;

  wlr_xdg_toplevel_decoration_v1_set_mode(
      decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}


} // namespace LunaDash
