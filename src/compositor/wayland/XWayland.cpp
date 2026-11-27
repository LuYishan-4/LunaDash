#include "compositor/client/ClientSurface.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/log/Log.hpp"
#include "core/log/StallTrace.hpp"
#include <algorithm>
#include <unistd.h>

namespace LunaDash {

void WaylandCompositor::Impl::addXWaylandSurface(
    wlr_xwayland_surface *surface) {
#if !LUDASH_WLR_HAS_XWAYLAND
  (void)surface;
#else
  if (!surface)
    return;

  auto client = std::make_unique<ClientWindow>();
  client->id = q->nextWindowId_++;
  client->type = ClientType::X11;
  client->surface.xwayland = surface;
  client->state.workspace = q->workspace_;
  client->processId = surface->pid;
  client->state.floating =
      surface->override_redirect || surface->parent || surface->modal;
  client->utility = surface->override_redirect;
  client->layout.preferredFloatingSize =
      Size(std::max<int>(1, surface->width), std::max<int>(1, surface->height));
  if (surface->override_redirect) {
    client->layout.manualGeometry =
        Rect(surface->x, surface->y, std::max<int>(1, surface->width),
             std::max<int>(1, surface->height));
  }

  auto *current = client.get();
  auto *state = new XWaylandState(this, current);
  surface->data = state;
  q->clients_.push_back(std::move(client));
  q->updateClientMetadata(current);
  state->listenToSurface();
  xwaylandSurfaces.push_back(state);
#endif
}

WaylandCompositor::Impl::XWaylandState::XWaylandState(Impl *owner,
                                                      ClientWindow *window)
    : impl(owner), client(window) {}

#if LUDASH_WLR_HAS_XWAYLAND
void WaylandCompositor::Impl::XWaylandState::listenToSurface() {
  auto &events = client->surface.xwayland->events;
  listen<&XWaylandState::onAssociate, &XWaylandState::associate>(
      &events.associate);
  listen<&XWaylandState::onDissociate, &XWaylandState::dissociate>(
      &events.dissociate);
  listen<&XWaylandState::onDestroy, &XWaylandState::destroy>(&events.destroy);
  listen<&XWaylandState::onConfigure, &XWaylandState::requestConfigure>(
      &events.request_configure);
  listen<&XWaylandState::onMove, &XWaylandState::requestMove>(
      &events.request_move);
  listen<&XWaylandState::onResize, &XWaylandState::requestResize>(
      &events.request_resize);
  listen<&XWaylandState::onMinimize, &XWaylandState::requestMinimize>(
      &events.request_minimize);
  listen<&XWaylandState::onMaximize, &XWaylandState::requestMaximize>(
      &events.request_maximize);
  listen<&XWaylandState::onFullscreen, &XWaylandState::requestFullscreen>(
      &events.request_fullscreen);
  listen<&XWaylandState::onActivate, &XWaylandState::requestActivate>(
      &events.request_activate);
  listen<&XWaylandState::onMetadata, &XWaylandState::setTitle>(
      &events.set_title);
  listen<&XWaylandState::onMetadata, &XWaylandState::setClass>(
      &events.set_class);
  listen<&XWaylandState::onMetadata, &XWaylandState::setParent>(
      &events.set_parent);
  listen<&XWaylandState::onMetadata, &XWaylandState::setGeometry>(
      &events.set_geometry);
  listen<&XWaylandState::onMetadata, &XWaylandState::setOverrideRedirect>(
      &events.set_override_redirect);
#if WLR_VERSION_MINOR >= 20
  listen<&XWaylandState::onIcon, &XWaylandState::setIcon>(&events.set_icon);
#endif
}

#if WLR_VERSION_MINOR >= 20
void WaylandCompositor::Impl::XWaylandState::onIcon() {
  impl->q->updateX11Icon(client);
}
#endif

void WaylandCompositor::Impl::XWaylandState::onAssociate() {
  auto *surface = clientSurface(*client);
  if (!surface)
    return;
  listen<&XWaylandState::onMap, &XWaylandState::map>(&surface->events.map);
  listen<&XWaylandState::onUnmap, &XWaylandState::unmap>(
      &surface->events.unmap);
  if (surface->mapped)
    onMap();
}

void WaylandCompositor::Impl::XWaylandState::onDissociate() {
  if (client->state.mapped)
    onUnmap();
  Templates::detachListeners(map, unmap);
  if (client->scene.tree) {
    wlr_scene_node_destroy(&client->scene.tree->node);
    client->scene.tree = nullptr;
    client->scene.content = nullptr;
  }
}

void WaylandCompositor::Impl::XWaylandState::onMap() {
  auto *xwayland = client->surface.xwayland;
  if (!xwayland || !xwayland->surface)
    return;
  StallTrace::Section trace("x11 map",
                            xwayland->class_ ? xwayland->class_ : "");
  if (client->scene.tree && !client->scene.content)
    wlr_scene_node_destroy(&client->scene.tree->node);
  if (!client->scene.tree) {
    client->scene.tree = wlr_scene_tree_create(impl->normalLayer);
    if (!client->scene.tree)
      return;
    client->scene.content =
        wlr_scene_subsurface_tree_create(client->scene.tree, xwayland->surface);
    if (!client->scene.content) {
      wlr_scene_node_destroy(&client->scene.tree->node);
      client->scene.tree = nullptr;
      return;
    }
    client->scene.tree->node.data = client;
    listen<&XWaylandState::onSceneContentDestroy,
           &XWaylandState::sceneContentDestroy>(
        &client->scene.content->node.events.destroy);
    listen<&XWaylandState::onSceneDestroy, &XWaylandState::sceneDestroy>(
        &client->scene.tree->node.events.destroy);
  }
  client->state.mapped = true;
  impl->finishSplash();
  auto *desktop = impl->q;
  desktop->updateClientMetadata(client);
  if (!client->utility)
    desktop->updateX11Icon(client);

  if (!client->layout.initialRuleApplied && !client->utility) {
    const auto rule =
        initialWindowRule(*desktop->pluginManager_,
                          {{"appId", client->appId},
                           {"title", client->title},
                           {"id", client->id}},
                          client->state.workspace, client->state.maximized,
                          desktop->workspaceCount());
    client->state.workspace = jsonInt(rule, "workspace");
    client->state.maximized = jsonBool(rule, "maximized");
  }
  client->layout.initialRuleApplied = true;
  impl->adoptShellDialog(client);
  desktop->arrange();
  if (!client->utility)
    desktop->focus(client);
}

void WaylandCompositor::Impl::XWaylandState::onUnmap() {
  StallTrace::Section trace("x11 unmap");
  auto *desktop = impl->q;
  if (impl->foreignToplevels)
    impl->foreignToplevels->remove(client->id);
  // Closing X11 windows shrink and fade out like Wayland ones (menus and
  // tooltips just vanish).
  if (desktop->windowAnimations_ && client->scene.tree &&
      client->state.mapped && !client->utility &&
      client->scene.tree->node.enabled) {
    desktop->windowAnimations_->close(client->scene.tree, impl->animationLayer);
    desktop->windowAnimations_->cancel(client->scene.tree);
  }
  if (impl->seat->keyboard_state.focused_surface == clientSurface(*client))
    wlr_seat_keyboard_notify_clear_focus(impl->seat);
  client->state.mapped = false;
  desktop->windowSwitcher_->remove(client->id);
  desktop->windowLayout_->remove(client->id);
  if (desktop->focused_ == client)
    desktop->focused_ = nullptr;
  desktop->arrange();
  desktop->synchronizeWindowFocus();
}

void WaylandCompositor::Impl::XWaylandState::onSceneContentDestroy() {
  Templates::detachListener(sceneContentDestroy);
  client->scene.content = nullptr;
}

void WaylandCompositor::Impl::XWaylandState::onSceneDestroy() {
  Templates::detachListener(sceneDestroy);
  // The wrapper is compositor-owned and can be destroyed before XWM emits
  // dissociate. Never leave layout or dissociate with a dangling tree.
  client->scene.tree = nullptr;
  client->scene.content = nullptr;
}

void WaylandCompositor::Impl::XWaylandState::onDestroy() {
  StallTrace::Section trace("x11 destroy");
  Impl *owner = impl;
  ClientWindow *window = client;
  if (owner->foreignToplevels)
    owner->foreignToplevels->remove(window->id);
  if (*window->iconFile)
    ::unlink(window->iconFile);

  // Scene lifetime is independent of XWM lifetime. The destroy listener clears
  // sceneTree when wlroots goes first; otherwise dispose of the live tree now
  // so node.data cannot outlive ClientWindow. Do not synthesize another unmap.
  if (owner->seat && clientSurface(*window) &&
      owner->seat->keyboard_state.focused_surface == clientSurface(*window))
    wlr_seat_keyboard_notify_clear_focus(owner->seat);
  window->state.mapped = false;
  if (window->scene.tree)
    wlr_scene_node_destroy(&window->scene.tree->node);
  window->scene.tree = nullptr;
  window->scene.content = nullptr;
  if (window->surface.xwayland)
    window->surface.xwayland->data = nullptr;
  window->surface.xwayland = nullptr;
  std::erase(owner->xwaylandSurfaces, this);
  delete this;

  // Keep ClientWindow alive until the current Wayland/XWM signal dispatch has
  // fully unwound. Scene nodes and other wlroots listeners may still be
  // finishing callbacks that carry this pointer as node.data.
  owner->deferClientRemoval(window);
}

void WaylandCompositor::Impl::XWaylandState::onConfigure(
    wlr_xwayland_surface_configure_event *event) {
  if (!event)
    return;
  const Size size(std::max<int>(1, event->width),
                   std::max<int>(1, event->height));
  client->layout.preferredFloatingSize = size;
  if (!client->state.mapped || client->state.floating || client->utility) {
    const Rect requested(event->x, event->y, size.width(), size.height());
    client->layout.manualGeometry = requested;
    wlr_xwayland_surface_configure(client->surface.xwayland, event->x, event->y,
                                   event->width, event->height);
    if (client->scene.tree)
      wlr_scene_node_set_position(&client->scene.tree->node, event->x,
                                  event->y);
    client->layout.geometry = requested;
    return;
  }
  impl->q->arrange();
  // Explicit ConfigureRequests need a reply even when tiling rejects a size.
  // Ordinary metadata/layout refreshes do not resend identical configurations.
  wlr_xwayland_surface_configure(
      client->surface.xwayland,
      static_cast<int16_t>(
          std::clamp(client->layout.geometry.x(), -32768, 32767)),
      static_cast<int16_t>(
          std::clamp(client->layout.geometry.y(), -32768, 32767)),
      static_cast<uint16_t>(
          std::clamp(client->layout.geometry.width(), 1, 65535)),
      static_cast<uint16_t>(
          std::clamp(client->layout.geometry.height(), 1, 65535)));
}

void WaylandCompositor::Impl::XWaylandState::onMove() {
  if (impl->beginClientWindowPointer(client, 0, 0, false))
    return;
  // A move request without an active pointer grab cannot be made into an
  // interactive exchange. Preserve the legacy fallback for such clients.
  client->state.floating = true;
  client->layout.manualGeometry = client->layout.geometry;
  impl->q->arrange();
}

void WaylandCompositor::Impl::XWaylandState::onResize() { onMove(); }

void WaylandCompositor::Impl::XWaylandState::onMinimize(
    wlr_xwayland_minimize_event *event) {
  if (!event || impl->q->refusesMinimize(*client, event->minimize))
    return;
  client->state.minimized = event->minimize;
  impl->q->windowLayout_->setMinimized(client->id, event->minimize);
  impl->q->arrange();
  impl->q->synchronizeWindowFocus();
}

void WaylandCompositor::Impl::XWaylandState::onMaximize() {
  if (const auto *xwayland = client->surface.xwayland)
    impl->q->setMaximized(client,
                          xwayland->maximized_horz && xwayland->maximized_vert);
}

void WaylandCompositor::Impl::XWaylandState::onFullscreen() {
  if (client->surface.xwayland)
    impl->q->setFullscreen(client, client->surface.xwayland->fullscreen);
}

void WaylandCompositor::Impl::XWaylandState::onActivate() {
  impl->q->focus(client);
}

void WaylandCompositor::Impl::XWaylandState::onMetadata() {
  const auto *xwayland = client->surface.xwayland;
  if (!xwayland)
    return;
  client->processId = xwayland->pid;
  impl->q->updateClientMetadata(client);
  if (client->utility) {
    client->state.floating = true;
    client->layout.manualGeometry =
        Rect(xwayland->x, xwayland->y, std::max<int>(1, xwayland->width),
             std::max<int>(1, xwayland->height));
  }
  // Property/class/geometry notifications are also emitted while XWM is
  // still constructing a window. Do not feed those short-lived, unmapped
  // helpers into the full workspace layout: the map handler will arrange the
  // window once its wl_surface is actually ready. This avoids configure/layout
  // churn for notification and recorder helper windows that may disappear
  // within the same X11 event burst.
  if (client->state.mapped)
    impl->q->arrange();
}
#endif

void WaylandCompositor::Impl::deferClientRemoval(ClientWindow *client) {
  if (!client || !eventLoop || !q)
    return;
  auto removal = std::make_unique<DeferredClientRemoval>();
  removal->impl = this;
  removal->client = client;
  auto *entry = removal.get();
  deferredClientRemovals.push_back(std::move(removal));
  auto *source =
      Templates::addTimer<&DeferredClientRemoval::onTimer>(eventLoop, entry);
  if (!source || wl_event_source_timer_update(source, 1) < 0) {
    if (source)
      wl_event_source_remove(source);
    deferredClientRemovals.pop_back();
    logCritical("Could not schedule deferred XWayland client removal.");
  }
}

void WaylandCompositor::Impl::DeferredClientRemoval::onTimer() {
  Impl *owner = impl;
  ClientWindow *window = client;
  auto &pending = owner->deferredClientRemovals;
  const auto self = std::find_if(pending.begin(), pending.end(),
                                 [this](const auto &entry) {
                                   return entry.get() == this;
                                 });
  if (self != pending.end())
    pending.erase(self); // `this` is gone from here on.
  auto &clients = owner->q->clients_;
  const auto found =
      std::find_if(clients.begin(), clients.end(),
                   [window](const auto &entry) { return entry.get() == window; });
  if (found != clients.end())
    owner->q->removeClient(window);
}

} // namespace LunaDash
