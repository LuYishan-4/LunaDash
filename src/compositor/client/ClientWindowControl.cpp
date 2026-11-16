#include "compositor/client/ClientWindowControl.hpp"

#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

namespace LunaDash {

bool clientReady(const ClientWindow *client) {
  if (!client || !client->wlSurface)
    return false;
  if (client->x11)
    return client->xwayland != nullptr;
  return client->surface && client->surface->initialized &&
         client->toplevel != nullptr;
}

void setClientActivated(ClientWindow *client, bool active) {
  if (!client)
    return;
#if LUDASH_WLR_HAS_XWAYLAND
  if (client->x11 && client->xwayland) {
    wlr_xwayland_surface_activate(client->xwayland, active);
    return;
  }
#endif
  if (client->toplevel)
    wlr_xdg_toplevel_set_activated(client->toplevel, active);
}

void closeClient(ClientWindow *client) {
  if (!client)
    return;
#if LUDASH_WLR_HAS_XWAYLAND
  if (client->x11 && client->xwayland) {
    wlr_xwayland_surface_close(client->xwayland);
    return;
  }
#endif
  if (client->toplevel)
    wlr_xdg_toplevel_send_close(client->toplevel);
}

void setClientMinimizedHint([[maybe_unused]] ClientWindow *client,
                            [[maybe_unused]] bool minimized) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (client && client->x11 && client->xwayland)
    wlr_xwayland_surface_set_minimized(client->xwayland, minimized);
#endif
}

bool isUtilityWindow(const std::string &appId) {
  return appId == "io.github.bugaevc.wl-clipboard";
}

} // namespace LunaDash
