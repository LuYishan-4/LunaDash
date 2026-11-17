#include "compositor/client/ClientWindowControl.hpp"

#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

#include <algorithm>
#include <cstdint>

namespace LunaDash {
namespace {

#if LUDASH_WLR_HAS_XWAYLAND
bool isXWaylandClient(const ClientWindow *client) {
  return client && client->x11 && client->xwayland;
}
#endif

// xdg-shell state requests before the initial commit would configure an
// uninitialized surface.
bool xdgToplevelConfigurable(const ClientWindow *client) {
  return client && client->toplevel && client->surface &&
         client->surface->initialized;
}

wlr_box xdgGeometry(wlr_xdg_surface *surface) {
  wlr_box geometry{};
#if WLR_VERSION_MINOR >= 19
  geometry = surface->geometry;
#else
  wlr_xdg_surface_get_geometry(surface, &geometry);
#endif
  return geometry;
}

#if LUDASH_WLR_HAS_XWAYLAND
void configureXWaylandSurface(ClientWindow *client, const Rect &rectangle,
                              const Size &size) {
  const auto x = static_cast<int16_t>(std::clamp(rectangle.x(), -32768, 32767));
  const auto y = static_cast<int16_t>(std::clamp(rectangle.y(), -32768, 32767));
  const auto width = static_cast<uint16_t>(std::clamp(size.width(), 1, 65535));
  const auto height =
      static_cast<uint16_t>(std::clamp(size.height(), 1, 65535));
  auto *surface = client->xwayland;
  if (surface->x != x || surface->y != y || surface->width != width ||
      surface->height != height)
    wlr_xwayland_surface_configure(surface, x, y, width, height);
  if (surface->maximized_horz != client->maximized ||
      surface->maximized_vert != client->maximized)
    WlrootsCompat::setXWaylandMaximized(surface, client->maximized);
  if (surface->fullscreen != client->fullscreen)
    wlr_xwayland_surface_set_fullscreen(surface, client->fullscreen);
  client->lastSize = size;
}

void readXWaylandMetadata(ClientWindow *client) {
  auto *surface = client->xwayland;
  client->title = safeUtf8(surface->title);
  client->appId =
      safeUtf8(surface->class_ ? surface->class_ : surface->instance);
  client->processId = surface->pid;
  client->utility = surface->override_redirect;
  if (!client->initialRuleApplied)
    client->floating = client->utility || surface->parent || surface->modal;
  else if (surface->parent || surface->modal)
    client->floating = true;
}
#endif

void configureXdgToplevel(ClientWindow *client, const Rect &rectangle,
                          const Size &size) {
  const wlr_box geometry = xdgGeometry(client->surface);
  wlr_box clip{geometry.x, geometry.y, rectangle.width(), rectangle.height()};
  if (client->sceneContent)
    wlr_scene_subsurface_tree_set_clip(&client->sceneContent->node,
                                       client->floating ? nullptr : &clip);
  if (client->lastSize != size) {
    client->lastSize = size;
    wlr_xdg_toplevel_set_size(client->toplevel, size.width(), size.height());
  }
  wlr_xdg_toplevel_set_maximized(client->toplevel, client->maximized);
  wlr_xdg_toplevel_set_fullscreen(client->toplevel, client->fullscreen);
  // Tell tiled clients that the size is not a suggestion. Without the tiled
  // state a toolkit may replace it with its own preferred size (winit does, so
  // Slint windows drew at their preferred size inside a larger tile).
  const std::uint32_t tiled =
      client->floating || client->maximized || client->fullscreen
          ? 0
          : WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT;
  if (client->toplevel->scheduled.tiled != tiled)
    wlr_xdg_toplevel_set_tiled(client->toplevel, tiled);
}

void readXdgMetadata(ClientWindow *client) {
  client->title = safeUtf8(client->toplevel->title);
  client->appId = safeUtf8(client->toplevel->app_id);
  client->utility = isUtilityWindow(client->appId);
  if (!client->initialRuleApplied)
    client->floating = client->toplevel->parent != nullptr;
  else if (client->toplevel->parent)
    client->floating = true;
}

} // namespace

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
  if (isXWaylandClient(client)) {
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
  if (isXWaylandClient(client)) {
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
  if (isXWaylandClient(client))
    wlr_xwayland_surface_set_minimized(client->xwayland, minimized);
#endif
}

void setClientMaximizedHint(ClientWindow *client, bool maximized) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    WlrootsCompat::setXWaylandMaximized(client->xwayland, maximized);
    return;
  }
#endif
  if (xdgToplevelConfigurable(client))
    wlr_xdg_toplevel_set_maximized(client->toplevel, maximized);
}

void setClientFullscreenHint(ClientWindow *client, bool fullscreen) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    wlr_xwayland_surface_set_fullscreen(client->xwayland, fullscreen);
    return;
  }
#endif
  if (xdgToplevelConfigurable(client))
    wlr_xdg_toplevel_set_fullscreen(client->toplevel, fullscreen);
}

void configureClientSurface(ClientWindow *client, const Rect &rectangle) {
  if (!clientReady(client))
    return;
  const Size size(std::max(1, rectangle.width()),
                  std::max(1, rectangle.height()));
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    configureXWaylandSurface(client, rectangle, size);
    return;
  }
#endif
  configureXdgToplevel(client, rectangle, size);
}

Size clientCommittedSize(const ClientWindow &client) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(&client))
    return Size(client.xwayland->width, client.xwayland->height);
#endif
  if (!client.surface)
    return Size();
  const wlr_box geometry = xdgGeometry(client.surface);
  return Size(geometry.width, geometry.height);
}

bool readClientProtocolMetadata(ClientWindow *client) {
  if (!client)
    return false;
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    readXWaylandMetadata(client);
    return true;
  }
#endif
  if (!client->toplevel)
    return false;
  readXdgMetadata(client);
  return true;
}

bool isUtilityWindow(const std::string &appId) {
  return appId == "io.github.bugaevc.wl-clipboard";
}

} // namespace LunaDash
