#include "compositor/client/ClientWindowControl.hpp"

#include "compositor/client/ClientSurface.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace LunaDash {
namespace {

#if LUDASH_WLR_HAS_XWAYLAND
bool isXWaylandClient(const ClientWindow *client) {
  return client && clientXWayland(*client);
}
#endif

bool xdgToplevelConfigurable(const ClientWindow *client) {
  auto *xdg = client ? clientXdg(*client) : nullptr;
  return xdg && xdg->toplevel && xdg->initialized;
}

wlr_scene_buffer_point_accepts_input_func_t surfaceAcceptsInput = nullptr;


bool naturalBufferBox(const wlr_scene_surface *scene, wlr_box *box) {
  const auto &state = scene->surface->current;
#if WLR_VERSION_MINOR >= 19
  const wlr_box &clip = scene->WLR_PRIVATE.clip;
#else
  const wlr_box &clip = scene->clip;
#endif
  *box = {0, 0, state.width, state.height};
  if (!wlr_box_empty(&clip)) {
    box->x = clip.x;
    box->y = clip.y;
    box->width = std::min(clip.width, state.width - clip.x);
    box->height = std::min(clip.height, state.height - clip.y);
  }
  return box->width > 0 && box->height > 0;
}

bool scaledSurfaceAcceptsInput(wlr_scene_buffer *buffer, double *sx, double *sy) {
  wlr_box natural{};
  auto *scene = wlr_scene_surface_try_from_buffer(buffer);
  if (scene && naturalBufferBox(scene, &natural) && buffer->dst_width > 0 &&
      buffer->dst_height > 0) {
    *sx = *sx * natural.width / buffer->dst_width;
    *sy = *sy * natural.height / buffer->dst_height;
  }
  return surfaceAcceptsInput ? surfaceAcceptsInput(buffer, sx, sy) : true;
}

int scaled(int value, double scale) {
  return static_cast<int>(std::lround(value * scale));
}

struct Scale {
  double x = 1.0;
  double y = 1.0;
};

void scaleSurfaceBuffers(wlr_scene_node *node, int x, int y, Scale scale) {
  if (node->type == WLR_SCENE_NODE_TREE) {
    auto *tree = wlr_scene_tree_from_node(node);
    wlr_scene_node *child = nullptr;
    wl_list_for_each(child, &tree->children, link) {
      if (child->type == WLR_SCENE_NODE_TREE)
        scaleSurfaceBuffers(child, x + child->x, y + child->y, scale);
      else
        scaleSurfaceBuffers(child, x, y, scale);
    }
    return;
  }
  if (node->type != WLR_SCENE_NODE_BUFFER)
    return;
  auto *buffer = wlr_scene_buffer_from_node(node);
  auto *scene = wlr_scene_surface_try_from_buffer(buffer);
  wlr_box natural{};
  if (!scene || !buffer->buffer || !naturalBufferBox(scene, &natural))
    return;
  if (buffer->point_accepts_input != scaledSurfaceAcceptsInput) {
    if (!surfaceAcceptsInput)
      surfaceAcceptsInput = buffer->point_accepts_input;
    buffer->point_accepts_input = scaledSurfaceAcceptsInput;
  }
  const int left = x + natural.x;
  const int top = y + natural.y;
  // Edges are rounded, not sizes, so neighbouring clip bands stay seamless.
  wlr_scene_node_set_position(node, scaled(left, scale.x) - x, scaled(top, scale.y) - y);
  wlr_scene_buffer_set_dest_size(
      buffer,
      std::max(1, scaled(left + natural.width, scale.x) - scaled(left, scale.x)),
      std::max(1, scaled(top + natural.height, scale.y) - scaled(top, scale.y)));
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
  auto *surface = clientXWayland(*client);
  if (surface->x != x || surface->y != y || surface->width != width ||
      surface->height != height)
    wlr_xwayland_surface_configure(surface, x, y, width, height);
  if (surface->maximized_horz != client->state.maximized ||
      surface->maximized_vert != client->state.maximized)
    WlrootsCompat::setXWaylandMaximized(surface, client->state.maximized);
  if (surface->fullscreen != client->state.fullscreen)
    wlr_xwayland_surface_set_fullscreen(surface, client->state.fullscreen);
  client->layout.lastSize = size;
}

void readXWaylandMetadata(ClientWindow *client) {
  auto *surface = clientXWayland(*client);
  setClientText(client->title, safeUtf8(surface->title));
  setClientText(client->appId, safeUtf8(surface->class_ ? surface->class_
                                                        : surface->instance));
  client->processId = surface->pid;
  client->utility = surface->override_redirect;
  if (!client->layout.initialRuleApplied)
    client->state.floating =
        client->utility || surface->parent || surface->modal;
  else if (surface->parent || surface->modal)
    client->state.floating = true;
}
#endif

void configureXdgToplevel(ClientWindow *client, const Rect &rectangle,
                          const Size &size) {
  auto *toplevel = clientToplevel(*client);
  const wlr_box geometry = xdgGeometry(clientXdg(*client));
  wlr_box clip{geometry.x, geometry.y, rectangle.width(), rectangle.height()};
  if (client->scene.content)
    wlr_scene_subsurface_tree_set_clip(
        &client->scene.content->node, client->state.floating ? nullptr : &clip);
  if (client->layout.lastSize != size) {
    client->layout.lastSize = size;
    wlr_xdg_toplevel_set_size(toplevel, size.width(), size.height());
  }
  wlr_xdg_toplevel_set_maximized(toplevel, client->state.maximized);
  wlr_xdg_toplevel_set_fullscreen(toplevel, client->state.fullscreen);

  const std::uint32_t tiled =
      client->state.floating || client->state.maximized ||
              client->state.fullscreen
          ? 0
          : WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT;
  if (toplevel->scheduled.tiled != tiled)
    wlr_xdg_toplevel_set_tiled(toplevel, tiled);
}

void readXdgMetadata(ClientWindow *client) {
  const auto *toplevel = clientToplevel(*client);
  setClientText(client->title, safeUtf8(toplevel->title));
  setClientText(client->appId, safeUtf8(toplevel->app_id));
  client->utility = isUtilityWindow(client->appId);
  if (!client->layout.initialRuleApplied)
    client->state.floating = toplevel->parent != nullptr;
  else if (toplevel->parent)
    client->state.floating = true;
}

} // namespace

bool clientReady(const ClientWindow *client) {
  if (!client || !clientSurface(*client))
    return false;
  if (clientIsX11(*client))
    return true;
  const auto *xdg = clientXdg(*client);
  return xdg && xdg->initialized && xdg->toplevel;
}

bool clientIsTransientOf(const ClientWindow *client,
                         const ClientWindow *ancestor,
                         std::size_t maxDepth) {
  if (!client || !ancestor || client == ancestor)
    return false;
#if LUDASH_WLR_HAS_XWAYLAND
  const auto *xwayland = clientXWayland(*client);
  const auto *ancestorXWayland = clientXWayland(*ancestor);
  if (xwayland && ancestorXWayland) {
    auto *parent = xwayland->parent;
    for (std::size_t depth = 0; parent && depth < maxDepth; ++depth) {
      if (parent == ancestorXWayland)
        return true;
      parent = parent->parent;
    }
    return false;
  }
#endif
  const auto *toplevel = clientToplevel(*client);
  const auto *ancestorToplevel = clientToplevel(*ancestor);
  if (toplevel && ancestorToplevel) {
    auto *parent = toplevel->parent;
    for (std::size_t depth = 0; parent && depth < maxDepth; ++depth) {
      if (parent == ancestorToplevel)
        return true;
      parent = parent->parent;
    }
  }
  return false;
}

void setClientActivated(ClientWindow *client, bool active) {
  if (!client)
    return;
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    wlr_xwayland_surface_activate(clientXWayland(*client), active);
    return;
  }
#endif
  if (clientToplevel(*client))
    wlr_xdg_toplevel_set_activated(clientToplevel(*client), active);
}

void closeClient(ClientWindow *client) {
  if (!client)
    return;
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    wlr_xwayland_surface_close(clientXWayland(*client));
    return;
  }
#endif
  if (clientToplevel(*client))
    wlr_xdg_toplevel_send_close(clientToplevel(*client));
}

void setClientMinimizedHint([[maybe_unused]] ClientWindow *client,
                            [[maybe_unused]] bool minimized) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client))
    wlr_xwayland_surface_set_minimized(clientXWayland(*client), minimized);
#endif
}

void setClientMaximizedHint(ClientWindow *client, bool maximized) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    WlrootsCompat::setXWaylandMaximized(clientXWayland(*client), maximized);
    return;
  }
#endif
  if (xdgToplevelConfigurable(client))
    wlr_xdg_toplevel_set_maximized(clientToplevel(*client), maximized);
}

void setClientFullscreenHint(ClientWindow *client, bool fullscreen) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    wlr_xwayland_surface_set_fullscreen(clientXWayland(*client), fullscreen);
    return;
  }
#endif
  if (xdgToplevelConfigurable(client))
    wlr_xdg_toplevel_set_fullscreen(clientToplevel(*client), fullscreen);
}

void applyClientContentScale(ClientWindow &client) {
  Scale scale{1.0, 1.0};
  // A resize animation stretches whatever the client last drew to the
  // animated size; it reaches 1:1 as the client's new size arrives.
  if (client.effects.animatedSize.width() > 0 &&
      client.effects.animatedSize.height() > 0) {
    const Size committed = clientCommittedSize(client);
    if (committed.width() > 0 && committed.height() > 0)
      scale = {static_cast<double>(client.effects.animatedSize.width()) /
                   committed.width(),
               static_cast<double>(client.effects.animatedSize.height()) /
                   committed.height()};
  }
  if (scale.x == 1.0 && scale.y == 1.0 && client.effects.scaleX == 1.0 &&
      client.effects.scaleY == 1.0)
    return;
  if (client.scene.tree)
    scaleSurfaceBuffers(&client.scene.tree->node, 0, 0, scale);
  client.effects.scaleX = scale.x;
  client.effects.scaleY = scale.y;
}

void configureClientSurface(ClientWindow *client, const Rect &rectangle) {
  if (!clientReady(client))
    return;
  const Size size(std::max(1, rectangle.width()), std::max(1, rectangle.height()));
  const Rect logical(rectangle.x(), rectangle.y(), size.width(), size.height());
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    configureXWaylandSurface(client, logical, size);
    return;
  }
#endif
  configureXdgToplevel(client, logical, size);
}

Size clientMinimumSize(const ClientWindow &client) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (const auto *xwayland = clientXWayland(client))
    return xwayland->size_hints
               ? Size(std::max(0, xwayland->size_hints->min_width),
                      std::max(0, xwayland->size_hints->min_height))
               : Size();
#endif
  if (const auto *toplevel = clientToplevel(client))
    return Size(toplevel->current.min_width, toplevel->current.min_height);
  return Size();
}

Size clientCommittedSize(const ClientWindow &client) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (const auto *xwayland = clientXWayland(client))
    return Size(xwayland->width, xwayland->height);
#endif
  auto *xdg = clientXdg(client);
  if (!xdg)
    return Size();
  const wlr_box geometry = xdgGeometry(xdg);
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
  if (!clientToplevel(*client))
    return false;
  readXdgMetadata(client);
  return true;
}

bool isUtilityWindow(const std::string &appId) {
  return appId == "io.github.bugaevc.wl-clipboard";
}

} // namespace LunaDash
