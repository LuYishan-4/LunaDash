#pragma once

// The xdg-shell / X11 split of a ClientWindow, kept in one place (as dwl's
// client.h): everything else asks these instead of reading `surface` itself.

#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

namespace LunaDash {

inline bool clientIsX11(const ClientWindow &client) {
  return client.type == ClientType::X11;
}

inline wlr_xdg_surface *clientXdg(const ClientWindow &client) {
  return clientIsX11(client) ? nullptr : client.surface.xdg;
}

inline wlr_xwayland_surface *clientXWayland(const ClientWindow &client) {
  return clientIsX11(client) ? client.surface.xwayland : nullptr;
}

inline wlr_xdg_toplevel *clientToplevel(const ClientWindow &client) {
  const auto *xdg = clientXdg(client);
  return xdg ? xdg->toplevel : nullptr;
}

inline wlr_surface *clientSurface(const ClientWindow &client) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (const auto *xwayland = clientXWayland(client))
    return xwayland->surface;
#endif
  const auto *xdg = clientXdg(client);
  return xdg ? xdg->surface : nullptr;
}

} // namespace LunaDash
