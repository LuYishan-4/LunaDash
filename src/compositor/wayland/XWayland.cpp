#include "compositor/wayland/Register.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <algorithm>

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
  client->x11 = true;
  client->xwayland = surface;
  client->wlSurface = surface->surface;
  client->workspace = q->workspace_;
  client->processId = surface->pid;
  client->floating = surface->override_redirect || surface->parent ||
                     surface->modal;
  client->utility = surface->override_redirect;
  client->preferredFloatingSize =
      Size(std::max<int>(1, surface->width), std::max<int>(1, surface->height));
  if (surface->override_redirect) {
    client->manualGeometry =
        Rect(surface->x, surface->y,
              std::max<int>(1, surface->width),
              std::max<int>(1, surface->height));
  }

  auto *current = client.get();
  auto *state = new XWaylandState(this, current, surface);
  current->nativeState = state;
  surface->data = state;
  q->clients_.push_back(std::move(client));
  q->updateClientMetadata(current);
  state->listenToSurface();
  xwaylandSurfaces.push_back(state);
#endif
}

WaylandCompositor::Impl::XWaylandState::XWaylandState(
    Impl *owner, ClientWindow *window, wlr_xwayland_surface *xwaylandSurface)
    : impl(owner), client(window), surface(xwaylandSurface) {}

#if LUDASH_WLR_HAS_XWAYLAND
void WaylandCompositor::Impl::XWaylandState::listenToSurface() {
  auto &events = surface->events;
  listen<&XWaylandState::onAssociate>(&events.associate, associate);
  listen<&XWaylandState::onDissociate>(&events.dissociate, dissociate);
  listen<&XWaylandState::onDestroy>(&events.destroy, destroy);
  listen<&XWaylandState::onConfigure>(&events.request_configure,
                                      requestConfigure);
  listen<&XWaylandState::onMove>(&events.request_move, requestMove);
  listen<&XWaylandState::onResize>(&events.request_resize, requestResize);
  listen<&XWaylandState::onMinimize>(&events.request_minimize, requestMinimize);
  listen<&XWaylandState::onMaximize>(&events.request_maximize, requestMaximize);
  listen<&XWaylandState::onFullscreen>(&events.request_fullscreen,
                                       requestFullscreen);
  listen<&XWaylandState::onActivate>(&events.request_activate, requestActivate);
  listen<&XWaylandState::onMetadata>(&events.set_title, setTitle);
  listen<&XWaylandState::onMetadata>(&events.set_class, setClass);
  listen<&XWaylandState::onMetadata>(&events.set_parent, setParent);
  listen<&XWaylandState::onMetadata>(&events.set_geometry, setGeometry);
  listen<&XWaylandState::onMetadata>(&events.set_override_redirect,
                                     setOverrideRedirect);
}

void WaylandCompositor::Impl::XWaylandState::onAssociate() {
  if (!surface || !surface->surface)
    return;
  client->wlSurface = surface->surface;
  listen<&XWaylandState::onMap>(&client->wlSurface->events.map, map);
  listen<&XWaylandState::onUnmap>(&client->wlSurface->events.unmap, unmap);
  if (client->wlSurface->mapped)
    onMap();
}

void WaylandCompositor::Impl::XWaylandState::onDissociate() {
  if (client->mapped)
    onUnmap();
  Templates::detachListeners(map, unmap);
  if (client->sceneTree) {
    wlr_scene_node_destroy(&client->sceneTree->node);
    client->sceneTree = nullptr;
    client->sceneContent = nullptr;
  }
  client->wlSurface = nullptr;
}

void WaylandCompositor::Impl::XWaylandState::onMap() {
  if (!surface || !surface->surface)
    return;
  if (!client->wlSurface)
    client->wlSurface = surface->surface;
  if (!client->wlSurface)
    return;
  if (client->sceneTree && !client->sceneContent)
    wlr_scene_node_destroy(&client->sceneTree->node);
  if (!client->sceneTree) {
    client->sceneTree = wlr_scene_tree_create(impl->normalLayer);
    if (!client->sceneTree)
      return;
    client->sceneContent =
        wlr_scene_subsurface_tree_create(client->sceneTree, client->wlSurface);
    if (!client->sceneContent) {
      wlr_scene_node_destroy(&client->sceneTree->node);
      client->sceneTree = nullptr;
      return;
    }
    client->sceneTree->node.data = client;
    listen<&XWaylandState::onSceneContentDestroy>(
        &client->sceneContent->node.events.destroy, sceneContentDestroy);
    listen<&XWaylandState::onSceneDestroy>(
        &client->sceneTree->node.events.destroy, sceneDestroy);
  }
  client->mapped = true;
  auto *desktop = impl->q;
  desktop->updateClientMetadata(client);

  if (!client->initialRuleApplied && !client->utility) {
    const auto rule = initialWindowRule(
        *desktop->pluginManager_,
        {{"appId", client->appId}, {"title", client->title}, {"id", client->id}},
        client->workspace, client->maximized, desktop->workspaceCount());
    client->workspace = jsonInt(rule, "workspace");
    client->maximized = jsonBool(rule, "maximized");
  }
  client->initialRuleApplied = true;
  desktop->arrange();
  if (!client->utility)
    desktop->focus(client);
}

void WaylandCompositor::Impl::XWaylandState::onUnmap() {
  auto *desktop = impl->q;
  if (impl->foreignToplevels)
    impl->foreignToplevels->remove(client->id);
  if (impl->seat->keyboard_state.focused_surface == client->wlSurface)
    wlr_seat_keyboard_notify_clear_focus(impl->seat);
  client->mapped = false;
  desktop->windowSwitcher_->remove(client->id);
  desktop->windowLayout_->remove(client->id);
  if (desktop->focused_ == client)
    desktop->focused_ = nullptr;
  desktop->arrange();
  desktop->synchronizeWindowFocus();
}

void WaylandCompositor::Impl::XWaylandState::onSceneContentDestroy() {
  Templates::detachListener(sceneContentDestroy);
  client->sceneContent = nullptr;
}

void WaylandCompositor::Impl::XWaylandState::onSceneDestroy() {
  Templates::detachListener(sceneDestroy);
  // The wrapper is compositor-owned and can be destroyed before XWM emits
  // dissociate. Never leave layout or dissociate with a dangling tree.
  client->sceneTree = nullptr;
  client->sceneContent = nullptr;
}

void WaylandCompositor::Impl::XWaylandState::onDestroy() {
  Impl *owner = impl;
  ClientWindow *window = client;
  if (owner->foreignToplevels)
    owner->foreignToplevels->remove(window->id);

  // Scene lifetime is independent of XWM lifetime. The destroy listener clears
  // sceneTree when wlroots goes first; otherwise dispose of the live tree now
  // so node.data cannot outlive ClientWindow. Do not synthesize another unmap.
  if (owner->seat && window->wlSurface &&
      owner->seat->keyboard_state.focused_surface == window->wlSurface)
    wlr_seat_keyboard_notify_clear_focus(owner->seat);
  window->mapped = false;
  if (window->sceneTree)
    wlr_scene_node_destroy(&window->sceneTree->node);
  window->wlSurface = nullptr;
  window->sceneTree = nullptr;
  window->sceneContent = nullptr;
  window->xwayland = nullptr;
  if (surface)
    surface->data = nullptr;
  std::erase(owner->xwaylandSurfaces, this);
  window->nativeState = nullptr;
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
  client->preferredFloatingSize = size;
  if (!client->mapped || client->floating || client->utility) {
    const Rect requested(event->x, event->y, size.width(), size.height());
    client->manualGeometry = requested;
    wlr_xwayland_surface_configure(surface, event->x, event->y, event->width,
                                   event->height);
    if (client->sceneTree)
      wlr_scene_node_set_position(&client->sceneTree->node, event->x, event->y);
    client->geometry = requested;
    return;
  }
  impl->q->arrange();
  // Explicit ConfigureRequests need a reply even when tiling rejects a size.
  // Ordinary metadata/layout refreshes do not resend identical configurations.
  wlr_xwayland_surface_configure(
      surface,
      static_cast<int16_t>(std::clamp(client->geometry.x(), -32768, 32767)),
      static_cast<int16_t>(std::clamp(client->geometry.y(), -32768, 32767)),
      static_cast<uint16_t>(std::clamp(client->geometry.width(), 1, 65535)),
      static_cast<uint16_t>(std::clamp(client->geometry.height(), 1, 65535)));
}

void WaylandCompositor::Impl::XWaylandState::onMove() {
  if (impl->beginClientWindowPointer(client, 0, 0, false))
    return;
  // A move request without an active pointer grab cannot be made into an
  // interactive exchange. Preserve the legacy fallback for such clients.
  client->floating = true;
  client->manualGeometry = client->geometry;
  impl->q->arrange();
}

void WaylandCompositor::Impl::XWaylandState::onResize() { onMove(); }

void WaylandCompositor::Impl::XWaylandState::onMinimize(
    wlr_xwayland_minimize_event *event) {
  if (!event)
    return;
  client->minimized = event->minimize;
  impl->q->windowLayout_->setMinimized(client->id, event->minimize);
  impl->q->arrange();
  impl->q->synchronizeWindowFocus();
}

void WaylandCompositor::Impl::XWaylandState::onMaximize() {
  if (surface)
    impl->q->setMaximized(client,
                          surface->maximized_horz && surface->maximized_vert);
}

void WaylandCompositor::Impl::XWaylandState::onFullscreen() {
  if (surface)
    impl->q->setFullscreen(client, surface->fullscreen);
}

void WaylandCompositor::Impl::XWaylandState::onActivate() {
  impl->q->focus(client);
}

void WaylandCompositor::Impl::XWaylandState::onMetadata() {
  if (!surface)
    return;
  client->processId = surface->pid;
  impl->q->updateClientMetadata(client);
  if (client->utility) {
    client->floating = true;
    client->manualGeometry =
        Rect(surface->x, surface->y, std::max<int>(1, surface->width),
             std::max<int>(1, surface->height));
  }
  // Property/class/geometry notifications are also emitted while XWM is
  // still constructing a window. Do not feed those short-lived, unmapped
  // helpers into the full workspace layout: the map handler will arrange the
  // window once its wl_surface is actually ready. This avoids configure/layout
  // churn for notification and recorder helper windows that may disappear
  // within the same X11 event burst.
  if (client->mapped)
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
