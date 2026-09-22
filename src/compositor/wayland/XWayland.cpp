#include "compositor/wayland/Register.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <algorithm>

namespace LunaDash {
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

void WaylandCompositor::Impl::addXWaylandSurface(
    wlr_xwayland_surface *surface) {
#if !LUDASH_WLR_HAS_XWAYLAND
  Q_UNUSED(surface)
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
      QSize(std::max<int>(1, surface->width), std::max<int>(1, surface->height));

  auto *current = client.get();
  auto *state = new XWaylandState;
  state->impl = this;
  state->client = current;
  state->surface = surface;
  current->nativeState = state;
  surface->data = state;
  q->clients_.push_back(std::move(client));
  q->updateClientMetadata(current);

  if (!surface->surface) {
    surface->data = nullptr;
    q->clients_.pop_back();
    delete state;
    return;
  }
  attachListener(&surface->surface->events.map, state->map, state,
                 handleXWaylandMap);
  attachListener(&surface->surface->events.unmap, state->unmap, state,
                 handleXWaylandUnmap);
  attachListener(&surface->events.destroy, state->destroy, state,
                 handleXWaylandDestroy);
  attachListener(&surface->events.request_configure, state->requestConfigure,
                 state, handleXWaylandConfigure);
  attachListener(&surface->events.request_move, state->requestMove, state,
                 handleXWaylandMove);
  attachListener(&surface->events.request_resize, state->requestResize, state,
                 handleXWaylandResize);
  attachListener(&surface->events.request_minimize, state->requestMinimize,
                 state, handleXWaylandMinimize);
  attachListener(&surface->events.request_maximize, state->requestMaximize,
                 state, handleXWaylandMaximize);
  attachListener(&surface->events.request_fullscreen, state->requestFullscreen,
                 state, handleXWaylandFullscreen);
  attachListener(&surface->events.request_activate, state->requestActivate,
                 state, handleXWaylandActivate);
  attachListener(&surface->events.set_title, state->setTitle, state,
                 handleXWaylandMetadata);
  attachListener(&surface->events.set_class, state->setClass, state,
                 handleXWaylandMetadata);
  attachListener(&surface->events.set_parent, state->setParent, state,
                 handleXWaylandMetadata);
  attachListener(&surface->events.set_geometry, state->setGeometry, state,
                 handleXWaylandMetadata);
  attachListener(&surface->events.set_override_redirect,
                 state->setOverrideRedirect, state, handleXWaylandMetadata);
  xwaylandSurfaces.append(state);
#endif
}

void WaylandCompositor::Impl::handleXWaylandMap(wl_listener *listener,
                                                 void *) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  if (!state || !state->client || !state->surface || !state->surface->surface)
    return;
  auto *client = state->client;
  client->wlSurface = state->surface->surface;
  if (!client->sceneTree) {
    client->sceneTree =
        wlr_scene_subsurface_tree_create(state->impl->normalLayer,
                                         client->wlSurface);
    if (!client->sceneTree)
      return;
    client->sceneTree->node.data = client;
  }
  client->mapped = true;
  state->impl->q->updateClientMetadata(client);

  if (!client->initialRuleApplied && !client->utility) {
    const auto rule = initialWindowRule(
        *state->impl->q->pluginManager_,
        {{"appId", client->appId}, {"title", client->title}, {"id", client->id}},
        client->workspace, client->maximized,
        desktopPreferences().value("workspaceCount").toInt());
    client->workspace = rule.value("workspace").toInt();
    client->maximized = rule.value("maximized").toBool();
  }
  client->initialRuleApplied = true;
  state->impl->q->arrange();
  if (!client->utility)
    state->impl->q->focus(client);
#endif
}

void WaylandCompositor::Impl::handleXWaylandUnmap(wl_listener *listener,
                                                   void *) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  if (!state || !state->client)
    return;
  auto *client = state->client;
  if (state->impl->seat->keyboard_state.focused_surface == client->wlSurface)
    wlr_seat_keyboard_notify_clear_focus(state->impl->seat);
  client->mapped = false;
  state->impl->q->windowSwitcher_->remove(client->id);
  state->impl->q->windowLayout_->remove(client->id);
  if (state->impl->q->focused_ == client)
    state->impl->q->focused_ = nullptr;
  state->impl->q->arrange();
  state->impl->q->synchronizeWindowFocus();
#endif
}

void WaylandCompositor::Impl::handleXWaylandDestroy(wl_listener *listener,
                                                     void *) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  if (!state || !state->client)
    return;
  auto *self = state->impl;
  auto *client = state->client;
  detachListener(state->map);
  detachListener(state->unmap);
  detachListener(state->destroy);
  detachListener(state->requestConfigure);
  detachListener(state->requestMove);
  detachListener(state->requestResize);
  detachListener(state->requestMinimize);
  detachListener(state->requestMaximize);
  detachListener(state->requestFullscreen);
  detachListener(state->requestActivate);
  detachListener(state->setTitle);
  detachListener(state->setClass);
  detachListener(state->setParent);
  detachListener(state->setGeometry);
  detachListener(state->setOverrideRedirect);
  if (state->surface)
    state->surface->data = nullptr;
  self->xwaylandSurfaces.removeAll(state);
  client->nativeState = nullptr;
  self->q->removeClient(client);
  delete state;
#endif
}

void WaylandCompositor::Impl::handleXWaylandConfigure(wl_listener *listener,
                                                       void *data) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  auto *event = static_cast<wlr_xwayland_surface_configure_event *>(data);
  if (!state || !state->client || !event)
    return;
  auto *client = state->client;
  const QSize size(std::max<int>(1, event->width),
                   std::max<int>(1, event->height));
  client->preferredFloatingSize = size;
  if (!client->mapped || client->floating || client->utility) {
    const QRect requested(event->x, event->y, size.width(), size.height());
    client->manualGeometry = requested;
    wlr_xwayland_surface_configure(state->surface, event->x, event->y,
                                   event->width, event->height);
    if (client->sceneTree)
      wlr_scene_node_set_position(&client->sceneTree->node, event->x, event->y);
    client->geometry = requested;
    return;
  }
  state->impl->q->arrange();
#endif
}

void WaylandCompositor::Impl::handleXWaylandMove(wl_listener *listener,
                                                  void *) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  if (state && state->client) {
    state->client->floating = true;
    state->client->manualGeometry = state->client->geometry;
    state->impl->q->arrange();
  }
#endif
}

void WaylandCompositor::Impl::handleXWaylandResize(wl_listener *listener,
                                                    void *data) {
  Q_UNUSED(data)
#if LUDASH_WLR_HAS_XWAYLAND
  handleXWaylandMove(listener, nullptr);
#endif
}

void WaylandCompositor::Impl::handleXWaylandMinimize(wl_listener *listener,
                                                      void *data) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  auto *event = static_cast<wlr_xwayland_minimize_event *>(data);
  if (!state || !state->client || !event)
    return;
  state->client->minimized = event->minimize;
  state->impl->q->windowLayout_->setMinimized(state->client->id,
                                               event->minimize);
  state->impl->q->arrange();
  state->impl->q->synchronizeWindowFocus();
#endif
}

void WaylandCompositor::Impl::handleXWaylandMaximize(wl_listener *listener,
                                                      void *) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  if (state && state->client && state->surface)
    state->impl->q->setMaximized(
        state->client,
        state->surface->maximized_horz && state->surface->maximized_vert);
#endif
}

void WaylandCompositor::Impl::handleXWaylandFullscreen(wl_listener *listener,
                                                        void *) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  if (state && state->client && state->surface)
    state->impl->q->setFullscreen(state->client, state->surface->fullscreen);
#endif
}

void WaylandCompositor::Impl::handleXWaylandActivate(wl_listener *listener,
                                                      void *) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  if (state && state->client)
    state->impl->q->focus(state->client);
#endif
}

void WaylandCompositor::Impl::handleXWaylandMetadata(wl_listener *listener,
                                                      void *) {
#if LUDASH_WLR_HAS_XWAYLAND
  auto *state = listenerOwner<XWaylandState>(listener);
  if (!state || !state->client || !state->surface)
    return;
  state->client->processId = state->surface->pid;
  state->impl->q->updateClientMetadata(state->client);
  state->impl->q->arrange();
#endif
}

} // namespace LunaDash
