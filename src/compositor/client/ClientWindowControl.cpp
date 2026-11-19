#include "compositor/client/ClientWindowControl.hpp"

#include "compositor/client/ClientWindow.hpp"
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
  return client && client->x11 && client->xwayland;
}
#endif

bool xdgToplevelConfigurable(const ClientWindow *client) {
  return client && client->toplevel && client->surface &&
         client->surface->initialized;
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

bool clientIsTransientOf(const ClientWindow *client,
                         const ClientWindow *ancestor,
                         std::size_t maxDepth) {
  if (!client || !ancestor || client == ancestor)
    return false;
#if LUDASH_WLR_HAS_XWAYLAND
  if (client->xwayland && ancestor->xwayland) {
    auto *parent = client->xwayland->parent;
    for (std::size_t depth = 0; parent && depth < maxDepth; ++depth) {
      if (parent == ancestor->xwayland)
        return true;
      parent = parent->parent;
    }
    return false;
  }
#endif
  if (client->toplevel && ancestor->toplevel) {
    auto *parent = client->toplevel->parent;
    for (std::size_t depth = 0; parent && depth < maxDepth; ++depth) {
      if (parent == ancestor->toplevel)
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

double clientContentScale(const ClientWindow &client) {
  return client.fullscreen || client.maximized ? 1.0 : client.contentScale;
}

Size clientLogicalSize(const ClientWindow &client, const Size &visual) {
  const double scale = clientContentScale(client);
  Size size(std::max(1, static_cast<int>(std::lround(visual.width() / scale))),
            std::max(1, static_cast<int>(std::lround(visual.height() / scale))));
  // A zoomed window's drawn size is rounded: keep the client's size rather
  // than send it a pixel of difference on every step of a zoom.
  if (scale != 1.0 && client.lastSize.width() > 0 && client.lastSize.height() > 0 &&
      std::abs(size.width() - client.lastSize.width()) <= 1 &&
      std::abs(size.height() - client.lastSize.height()) <= 1)
    return client.lastSize;
  return size;
}

void applyClientContentScale(ClientWindow &client) {
  const double content = clientContentScale(client);
  Scale scale{content, content};
  // A resize animation stretches whatever the client last drew to the
  // animated size; it reaches 1:1 as the client's new size arrives.
  if (client.animatedSize.width() > 0 && client.animatedSize.height() > 0) {
    const Size committed = clientCommittedSize(client);
    if (committed.width() > 0 && committed.height() > 0)
      scale = {static_cast<double>(client.animatedSize.width()) / committed.width(),
               static_cast<double>(client.animatedSize.height()) / committed.height()};
  }
  if (scale.x == 1.0 && scale.y == 1.0 && client.appliedScaleX == 1.0 &&
      client.appliedScaleY == 1.0)
    return;
  if (client.sceneTree)
    scaleSurfaceBuffers(&client.sceneTree->node, 0, 0, scale);
  client.appliedScaleX = scale.x;
  client.appliedScaleY = scale.y;
}

void configureClientSurface(ClientWindow *client, const Rect &rectangle) {
  if (!clientReady(client))
    return;
  // The client lays itself out at its logical size; a zoomed window is then
  // drawn larger or smaller (applyClientContentScale).
  const Size size = clientLogicalSize(
      *client, Size(std::max(1, rectangle.width()), std::max(1, rectangle.height())));
  const Rect logical(rectangle.x(), rectangle.y(), size.width(), size.height());
#if LUDASH_WLR_HAS_XWAYLAND
  if (isXWaylandClient(client)) {
    configureXWaylandSurface(client, logical, size);
    return;
  }
#endif
  configureXdgToplevel(client, logical, size);
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
