#include "compositor/animation/SceneWindowAnimations.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/Runtime.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include <QTimer>
#include <algorithm>

namespace LunaDash {
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

QString safeUtf8(const char *text) {
  return QString::fromUtf8(text ? text : "");
}
void WaylandCompositor::Impl::configureInitialToplevel(ClientWindow *client) {
  if (!client || !client->surface || !client->toplevel ||
      !client->surface->initialized)
    return;
  wlr_xdg_toplevel_set_size(client->toplevel, 0, 0);
  if (client->toplevel->requested.maximized)
    wlr_xdg_toplevel_set_maximized(client->toplevel, true);
  if (client->toplevel->requested.fullscreen)
    wlr_xdg_toplevel_set_fullscreen(client->toplevel, true);
}

void WaylandCompositor::Impl::addXdgToplevel(wlr_xdg_surface *surface,
                                             wlr_xdg_toplevel *toplevel) {
  if (!surface || !toplevel)
    return;

  auto client = std::make_unique<ClientWindow>();
  client->id = q->nextWindowId_++;
  client->surface = surface;
  client->toplevel = toplevel;
  client->workspace = q->workspace_;
  client->sceneTree = wlr_scene_xdg_surface_create(normalLayer, surface);
  if (!client->sceneTree)
    return;

  client->sceneTree->node.data = client.get();
  if (surface->client && surface->client->client) {
    pid_t pid = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    wl_client_get_credentials(surface->client->client, &pid, &uid, &gid);
    client->processId = static_cast<qint64>(pid);
  }

  auto *current = client.get();
  auto *state = new ToplevelState;
  state->impl = this;
  state->client = current;
  current->nativeState = state;
  q->clients_.push_back(std::move(client));
  q->updateClientMetadata(current);

  attachListener(&surface->surface->events.map, state->map, state,
                 handleToplevelMap);
  attachListener(&surface->surface->events.unmap, state->unmap, state,
                 handleToplevelUnmap);
  attachListener(&surface->surface->events.commit, state->commit, state,
                 handleToplevelCommit);
  attachListener(WlrootsCompat::xdgToplevelDestroySignal(surface, toplevel),
                 state->destroy, state, handleToplevelDestroy);
  attachListener(&toplevel->events.set_title, state->setTitle, state,
                 handleToplevelMetadata);
  attachListener(&toplevel->events.set_app_id, state->setAppId, state,
                 handleToplevelMetadata);
  attachListener(&toplevel->events.set_parent, state->setParent, state,
                 handleToplevelParent);
  attachListener(&toplevel->events.request_minimize, state->requestMinimize,
                 state, handleToplevelMinimize);
  attachListener(&toplevel->events.request_maximize, state->requestMaximize,
                 state, handleToplevelMaximize);
  attachListener(&toplevel->events.request_fullscreen, state->requestFullscreen,
                 state, handleToplevelFullscreen);
  wlr_scene_node_set_enabled(&current->sceneTree->node, false);
#if WLR_VERSION_MINOR < 18
  // wlroots 0.17 emits xdg_shell.new_surface from the role's first commit.
  // The commit listener is installed too late for that commit, so configure
  // immediately while initial_commit is still set.
  if (surface->initial_commit)
    configureInitialToplevel(current);
#endif
}

#if WLR_VERSION_MINOR < 18
void WaylandCompositor::Impl::handleNewXdgSurface(wl_listener *listener,
                                                  void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *surface = static_cast<wlr_xdg_surface *>(data);
  if (!self || !surface || surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL ||
      !surface->toplevel)
    return;
  self->addXdgToplevel(surface, surface->toplevel);
}
#endif

#if WLR_VERSION_MINOR >= 18
void WaylandCompositor::Impl::handleNewXdgToplevel(wl_listener *listener,
                                                   void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *toplevel = static_cast<wlr_xdg_toplevel *>(data);
  if (!self || !toplevel || !toplevel->base)
    return;
  self->addXdgToplevel(toplevel->base, toplevel);
}
#endif

void WaylandCompositor::Impl::handleToplevelMap(wl_listener *listener, void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  auto *client = state->client;
  client->mapped = true;
  client->initialRuleApplied = true;
  state->impl->q->updateClientMetadata(client);
  state->impl->q->arrange();
  if (state->impl->q->windowAnimations_ && client->sceneTree &&
      !client->utility)
    state->impl->q->windowAnimations_->show(client->sceneTree,
                                            client->geometry.topLeft());
  if (!client->utility)
    state->impl->q->focus(client);
}

void WaylandCompositor::Impl::handleToplevelUnmap(wl_listener *listener,
                                                  void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  if (state->impl->q->windowAnimations_ && state->client->sceneTree)
    state->impl->q->windowAnimations_->hideSnapshot(
        state->client->sceneTree, state->impl->animationLayer);
  if (state->impl->q->windowAnimations_ && state->client->sceneTree)
    state->impl->q->windowAnimations_->cancel(state->client->sceneTree);
  if (state->impl->seat->keyboard_state.focused_surface ==
      state->client->surface->surface)
    wlr_seat_keyboard_notify_clear_focus(state->impl->seat);
  state->client->mapped = false;
  state->impl->q->tiling_.remove(state->client->id);
  if (state->impl->q->focused_ == state->client)
    state->impl->q->focused_ = nullptr;
  state->impl->q->arrange();
  state->impl->q->synchronizeTilingFocus();
}

void WaylandCompositor::Impl::handleToplevelCommit(wl_listener *listener,
                                                   void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client || !state->client->surface ||
      !state->client->toplevel)
    return;
  if (!state->client->surface->initial_commit)
    return;

  // Requests such as set_maximized can arrive before the first surface
  // commit. Apply them only after wlroots marks the role initialized.
  configureInitialToplevel(state->client);
}

void WaylandCompositor::Impl::handleToplevelMetadata(wl_listener *listener,
                                                     void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  state->impl->q->updateClientMetadata(state->client);
  state->impl->q->arrange();
}

void WaylandCompositor::Impl::handleToplevelParent(wl_listener *listener,
                                                   void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  state->impl->q->updateClientMetadata(state->client);
  if (state->client->floating)
    state->impl->q->tiling_.remove(state->client->id);
  state->impl->q->arrange();
}

void WaylandCompositor::Impl::handleToplevelMinimize(wl_listener *listener,
                                                     void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  state->client->minimized = true;
  state->impl->q->tiling_.setMinimized(state->client->id, true);
  state->impl->q->arrange();
  state->impl->q->synchronizeTilingFocus();
}

void WaylandCompositor::Impl::handleToplevelMaximize(wl_listener *listener,
                                                     void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client || !state->client->surface ||
      !state->client->surface->initialized)
    return;
  state->client->maximized = state->client->toplevel->requested.maximized;
  wlr_xdg_toplevel_set_maximized(state->client->toplevel,
                                 state->client->maximized);
  state->impl->q->arrange();
}

void WaylandCompositor::Impl::handleToplevelFullscreen(wl_listener *listener,
                                                       void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client || !state->client->surface ||
      !state->client->surface->initialized)
    return;
  const bool fullscreen = state->client->toplevel->requested.fullscreen;
  state->client->maximized = fullscreen;
  wlr_xdg_toplevel_set_fullscreen(state->client->toplevel, fullscreen);
  state->impl->q->arrange();
}

void WaylandCompositor::Impl::handleToplevelDestroy(wl_listener *listener,
                                                    void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  auto *self = state->impl;
  ClientWindow *client = state->client;
  detachListener(state->map);
  detachListener(state->unmap);
  detachListener(state->commit);
  detachListener(state->destroy);
  detachListener(state->setTitle);
  detachListener(state->setAppId);
  detachListener(state->setParent);
  detachListener(state->requestMinimize);
  detachListener(state->requestMaximize);
  detachListener(state->requestFullscreen);
  client->nativeState = nullptr;
  self->q->removeClient(client);
  delete state;
}

void WaylandCompositor::Impl::handleNewLayerSurface(wl_listener *listener,
                                                    void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *surface = static_cast<wlr_layer_surface_v1 *>(data);
  if (!self || !surface)
    return;
  if (!surface->output)
    surface->output = self->primaryOutput;

  wlr_scene_tree *parent = self->topLayer;
  switch (surface->pending.layer) {
  case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
    parent = self->backgroundLayer;
    break;
  case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
    parent = self->bottomLayer;
    break;
  case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
    parent = self->topLayer;
    break;
  case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
    parent = self->overlayLayer;
    break;
  }

  auto *state = new LayerState;
  state->impl = self;
  state->surface = surface;
  state->sceneLayer = wlr_scene_layer_surface_v1_create(parent, surface);
  if (!state->sceneLayer) {
    delete state;
    return;
  }
  self->layers.append(state);
  attachListener(&surface->surface->events.map, state->map, state,
                 handleLayerMap);
  attachListener(&surface->surface->events.unmap, state->unmap, state,
                 handleLayerUnmap);
  attachListener(&surface->surface->events.commit, state->commit, state,
                 handleLayerCommit);
  attachListener(&surface->events.destroy, state->destroy, state,
                 handleLayerDestroy);
  // The first surface commit marks the role initialized; handleLayerCommit
  // will perform the initial configure/layout at that point.
}

void WaylandCompositor::Impl::handleLayerMap(wl_listener *listener, void *) {
  auto *state = listenerOwner<LayerState>(listener);
  if (!state)
    return;
  state->mapped = true;
  state->impl->arrangeLayers();
  state->impl->q->arrange();
  if (state->surface && state->surface->current.keyboard_interactive !=
                            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
    state->impl->focusSurface(state->surface->surface);
}

void WaylandCompositor::Impl::handleLayerUnmap(wl_listener *listener, void *) {
  auto *state = listenerOwner<LayerState>(listener);
  if (!state)
    return;
  state->mapped = false;
  state->impl->arrangeLayers();
  state->impl->q->arrange();
}

void WaylandCompositor::Impl::handleLayerCommit(wl_listener *listener, void *) {
  auto *state = listenerOwner<LayerState>(listener);
  if (!state || !state->surface || !state->surface->initialized)
    return;
  state->impl->arrangeLayers();
  state->impl->q->arrange();
  if (state->mapped && state->surface &&
      state->surface->current.keyboard_interactive ==
          ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
    state->impl->focusSurface(state->surface->surface);
}

void WaylandCompositor::Impl::handleLayerDestroy(wl_listener *listener,
                                                 void *) {
  auto *state = listenerOwner<LayerState>(listener);
  if (!state)
    return;
  auto *self = state->impl;
  detachListener(state->map);
  detachListener(state->unmap);
  detachListener(state->commit);
  detachListener(state->destroy);
  self->layers.removeAll(state);
  delete state;
  self->arrangeLayers();
  self->q->arrange();
}
ClientWindow *
WaylandCompositor::Impl::clientForSurface(wlr_surface *surface) const {
  if (!surface || !surface->resource)
    return nullptr;
  wl_client *owner = wl_resource_get_client(surface->resource);
  for (const auto &client : q->clients_) {
    if (!client->surface || !client->surface->resource)
      continue;
    if (wl_resource_get_client(client->surface->resource) == owner)
      return client.get();
  }
  return nullptr;
}

} // namespace LunaDash
