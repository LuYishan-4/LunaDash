#include "compositor/window/animation/WindowAnimation.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <QTimer>
#include <algorithm>

namespace LunaDash {
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

QString safeUtf8(const char *text) {
  return QString::fromUtf8(text ? text : "");
}

#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
void WaylandCompositor::Impl::updateForeignToplevel(ToplevelState *state) {
  if (!state || !state->foreignHandle || !state->client)
    return;
  const QByteArray title = state->client->title.toUtf8();
  const QByteArray appId = state->client->appId.toUtf8();
  const wlr_ext_foreign_toplevel_handle_v1_state foreignState{
      .title = title.constData(),
      .app_id = appId.constData(),
  };
  wlr_ext_foreign_toplevel_handle_v1_update_state(state->foreignHandle,
                                                   &foreignState);
}

void WaylandCompositor::Impl::createForeignToplevel(ToplevelState *state) {
  if (!state || state->foreignHandle || !state->client ||
      !state->client->mapped || !state->impl->foreignToplevelList)
    return;
  const QByteArray title = state->client->title.toUtf8();
  const QByteArray appId = state->client->appId.toUtf8();
  const wlr_ext_foreign_toplevel_handle_v1_state foreignState{
      .title = title.constData(),
      .app_id = appId.constData(),
  };
  state->foreignHandle = wlr_ext_foreign_toplevel_handle_v1_create(
      state->impl->foreignToplevelList, &foreignState);
  if (state->foreignHandle)
    state->foreignHandle->data = state;
}

void WaylandCompositor::Impl::destroyForeignToplevel(ToplevelState *state) {
  if (!state || !state->foreignHandle)
    return;
  state->foreignHandle->data = nullptr;
  wlr_ext_foreign_toplevel_handle_v1_destroy(state->foreignHandle);
  state->foreignHandle = nullptr;
}
#endif


#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
void WaylandCompositor::Impl::updateClientLegacyForeignToplevel(
    ClientWindow *client) {
  // nativeState holds XWaylandState for X11 clients. Its listener pointers
  // must never be interpreted as ToplevelState's foreign protocol handles.
  if (!client || client->x11 || !client->nativeState)
    return;
  updateLegacyForeignToplevel(static_cast<ToplevelState *>(client->nativeState));
}

void WaylandCompositor::Impl::updateLegacyForeignToplevel(
    ToplevelState *state) {
  if (!state || !state->legacyForeignHandle || !state->client)
    return;
  const QByteArray title = state->client->title.toUtf8();
  const QByteArray appId = state->client->appId.toUtf8();
  wlr_foreign_toplevel_handle_v1_set_title(state->legacyForeignHandle,
                                            title.constData());
  wlr_foreign_toplevel_handle_v1_set_app_id(state->legacyForeignHandle,
                                             appId.constData());
  wlr_foreign_toplevel_handle_v1_set_activated(
      state->legacyForeignHandle,
      state->client->mapped && !state->client->minimized &&
          state->impl->q->focused_ == state->client);
  wlr_foreign_toplevel_handle_v1_set_minimized(state->legacyForeignHandle,
                                                state->client->minimized);
  wlr_foreign_toplevel_handle_v1_set_maximized(state->legacyForeignHandle,
                                                state->client->maximized);
  wlr_foreign_toplevel_handle_v1_set_fullscreen(state->legacyForeignHandle,
                                                 state->client->fullscreen);
}

void WaylandCompositor::Impl::createLegacyForeignToplevel(
    ToplevelState *state) {
  if (!state || state->legacyForeignHandle || !state->client ||
      !state->client->mapped || !state->impl->foreignToplevelManager)
    return;
  state->legacyForeignHandle = wlr_foreign_toplevel_handle_v1_create(
      state->impl->foreignToplevelManager);
  if (!state->legacyForeignHandle)
    return;
  state->legacyForeignHandle->data = state;
  updateLegacyForeignToplevel(state);
}

void WaylandCompositor::Impl::destroyLegacyForeignToplevel(
    ToplevelState *state) {
  if (!state || !state->legacyForeignHandle)
    return;
  state->legacyForeignHandle->data = nullptr;
  wlr_foreign_toplevel_handle_v1_destroy(state->legacyForeignHandle);
  state->legacyForeignHandle = nullptr;
}
#endif
void WaylandCompositor::Impl::configureInitialToplevel(ClientWindow *client) {
  if (!client || !client->surface || !client->toplevel ||
      !client->surface->initialized)
    return;
  q->updateClientMetadata(client);
  q->arrange();
  client->fullscreen = client->toplevel->requested.fullscreen;
  client->maximized = client->toplevel->requested.maximized;
  if (!client->initialRuleApplied && !client->utility && !client->floating) {
    const auto rule = initialWindowRule(
        *q->pluginManager_,
        {{"appId", client->appId}, {"title", client->title}, {"id", client->id}},
        client->workspace, client->maximized,
        desktopPreferences().value("workspaceCount").toInt());
    client->workspace = rule.value("workspace").toInt();
    client->maximized = rule.value("maximized").toBool();
  }
  client->initialRuleApplied = true;
  client->lastSize = {};
  client->layoutPending = q->windowTemplate_ &&
                          !q->windowTemplate_->allowOverlap &&
                          !client->floating && !client->utility &&
                          !client->fullscreen && !client->maximized;
  if (client->layoutPending) {
    // Use the real template membership, including other pending windows, so
    // mapping the first buffer cannot insert the window a second time. A 0x0
    // configure followed by tiling at map time reflows startup terminal output
    // and needlessly rebuilds other clients' freshly drawn layouts.
    for (const auto &peer : q->clients_)
      if (peer.get() != client && peer->workspace == client->workspace &&
          !peer->floating && peer->maximized)
        q->setMaximized(peer.get(), false);
    q->arrange();
    if (client->lastSize.isValid())
      return;
  } else {
    // A pre-map maximize/fullscreen request can replace a reserved tile.
    q->arrange();
  }
  const QSize size = client->fullscreen ? outputSize() : q->workArea().size();
  const bool constrained = client->fullscreen || client->maximized;
  wlr_xdg_toplevel_set_size(client->toplevel,
                            constrained ? std::max(1, size.width()) : 0,
                            constrained ? std::max(1, size.height()) : 0);
  wlr_xdg_toplevel_set_maximized(client->toplevel, client->maximized);
  wlr_xdg_toplevel_set_fullscreen(client->toplevel, client->fullscreen);
}

void WaylandCompositor::Impl::addXdgToplevel(wlr_xdg_surface *surface,
                                             wlr_xdg_toplevel *toplevel) {
  if (!surface || !toplevel)
    return;

  auto client = std::make_unique<ClientWindow>();
  client->id = q->nextWindowId_++;
  client->surface = surface;
  client->toplevel = toplevel;
  client->wlSurface = surface->surface;
  client->workspace = q->workspace_;
  client->sceneTree = wlr_scene_tree_create(normalLayer);
  if (!client->sceneTree)
    return;
  client->sceneContent = wlr_scene_xdg_surface_create(client->sceneTree, surface);
  if (!client->sceneContent) {
    wlr_scene_node_destroy(&client->sceneTree->node);
    return;
  }

  client->sceneTree->node.data = client.get();
  surface->data = client->sceneTree;
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
  attachListener(
      &current->sceneContent->node.events.destroy, state->sceneContentDestroy,
      state, [](wl_listener *listener, void *) {
        auto *state = listenerOwner<ToplevelState>(listener);
        detachListener(state->sceneContentDestroy);
        if (state->client)
          state->client->sceneContent = nullptr;
      });
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  // Window capture gets a private scene so the stream does not inherit
  // workspace transforms, tiling clips or visibility of the desktop scene.
  state->imageCaptureScene = wlr_scene_create();
  if (state->imageCaptureScene) {
    state->imageCaptureTree = wlr_scene_xdg_surface_create(
        &state->imageCaptureScene->tree, surface);
    if (!state->imageCaptureTree) {
      wlr_scene_node_destroy(&state->imageCaptureScene->tree.node);
      state->imageCaptureScene = nullptr;
    }
  }
#endif
  q->clients_.push_back(std::move(client));
  q->updateClientMetadata(current);

  attachListener(&surface->surface->events.map, state->map, state,
                 handleToplevelMap);
  attachListener(&surface->surface->events.unmap, state->unmap, state,
                 handleToplevelUnmap);
  // Capture the last mapped frame before wlroots disables the subsurface
  // tree. The surface still owns its buffer at the start of the unmap signal.
  wl_list_remove(&state->unmap.listener.link);
  wl_list_insert(&surface->surface->events.unmap.listener_list,
                 &state->unmap.listener.link);
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
  attachListener(&toplevel->events.request_move, state->requestMove, state,
                 handleToplevelMove);
  attachListener(&toplevel->events.request_resize, state->requestResize, state,
                 handleToplevelResize);
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
  if (!self || !surface)
    return;
  if (surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && surface->toplevel)
    self->addXdgToplevel(surface, surface->toplevel);
  else if (surface->role == WLR_XDG_SURFACE_ROLE_POPUP && surface->popup)
    self->addXdgPopup(surface->popup);
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
  client->layoutPending = false;
  state->impl->q->updateClientMetadata(client);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  // Only mapped windows belong in the portal's selectable toplevel list.
  createForeignToplevel(state);
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  createLegacyForeignToplevel(state);
#endif
  // A new ordinary window joins the visible layout instead of inheriting zoom.
  if (state->impl->q->windowTemplate_ &&
      !state->impl->q->windowTemplate_->allowOverlap && !client->floating &&
      !client->utility)
    for (const auto &peer : state->impl->q->clients_)
      if (peer.get() != client && peer->workspace == client->workspace &&
          !peer->floating && peer->maximized)
        state->impl->q->setMaximized(peer.get(), false);
  state->impl->q->arrange();
  if (!client->utility)
    state->impl->q->focus(client);
  if (state->impl->q->windowAnimations_ && client->sceneTree &&
      !client->utility)
    state->impl->q->windowAnimations_->open(client->sceneTree,
                                            client->geometry);
}

void WaylandCompositor::Impl::handleToplevelUnmap(wl_listener *listener,
                                                  void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  if (state->impl->q->windowAnimations_ && state->client->sceneTree)
    state->impl->q->windowAnimations_->close(
        state->client->sceneTree, state->impl->animationLayer);
  if (state->impl->q->windowAnimations_ && state->client->sceneTree)
    state->impl->q->windowAnimations_->cancel(state->client->sceneTree);
  if (state->impl->seat->keyboard_state.focused_surface ==
      state->client->wlSurface)
    wlr_seat_keyboard_notify_clear_focus(state->impl->seat);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  destroyForeignToplevel(state);
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  destroyLegacyForeignToplevel(state);
#endif
  state->client->mapped = false;
  state->client->layoutPending = false;
  state->client->initialRuleApplied = false;
  state->client->preferredFloatingSize = {};
  state->impl->q->windowSwitcher_->remove(state->client->id);
  if (state->impl->pointerWindow == state->client->id)
    state->impl->finishWindowPointer(false);
  state->impl->q->windowLayout_->remove(state->client->id);
  if (state->impl->q->focused_ == state->client)
    state->impl->q->focused_ = nullptr;
  state->impl->q->arrange();
  state->impl->q->synchronizeWindowFocus();
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
  state->impl->configureInitialToplevel(state->client);
}

void WaylandCompositor::Impl::handleToplevelMetadata(wl_listener *listener,
                                                     void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  state->impl->q->updateClientMetadata(state->client);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  updateForeignToplevel(state);
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  updateLegacyForeignToplevel(state);
#endif
  state->impl->q->arrange();
}

void WaylandCompositor::Impl::handleToplevelParent(wl_listener *listener,
                                                   void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  state->impl->q->updateClientMetadata(state->client);
  if (state->client->floating)
    state->impl->q->windowLayout_->remove(state->client->id);
  state->impl->q->arrange();
}

void WaylandCompositor::Impl::handleToplevelMinimize(wl_listener *listener,
                                                     void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client)
    return;
  state->client->minimized = true;
  state->impl->q->windowLayout_->setMinimized(state->client->id, true);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  updateLegacyForeignToplevel(state);
#endif
  state->impl->q->arrange();
  state->impl->q->synchronizeWindowFocus();
}

void WaylandCompositor::Impl::handleToplevelMaximize(wl_listener *listener,
                                                     void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client || !state->client->surface ||
      !state->client->surface->initialized)
    return;
  if (!state->client->mapped) {
    state->impl->configureInitialToplevel(state->client);
    return;
  }
  state->impl->q->setMaximized(state->client,
                               state->client->toplevel->requested.maximized);
  state->impl->q->arrange();
  if (state->client->maximized)
    state->impl->q->focus(state->client);
}

void WaylandCompositor::Impl::handleToplevelFullscreen(wl_listener *listener,
                                                       void *) {
  auto *state = listenerOwner<ToplevelState>(listener);
  if (!state || !state->client || !state->client->surface ||
      !state->client->surface->initialized)
    return;
  if (!state->client->mapped) {
    state->impl->configureInitialToplevel(state->client);
    return;
  }
  const bool fullscreen = state->client->toplevel->requested.fullscreen;
  state->impl->q->setFullscreen(state->client, fullscreen);
}

void WaylandCompositor::Impl::handleToplevelMove(wl_listener *listener,
                                                 void *data) {
  auto *state = listenerOwner<ToplevelState>(listener);
  auto *event = static_cast<wlr_xdg_toplevel_move_event *>(data);
  if (state && event && event->seat && event->seat->seat == state->impl->seat)
    state->impl->beginClientWindowPointer(state->client, event->serial, 0);
}
void WaylandCompositor::Impl::handleToplevelResize(wl_listener *listener,
                                                   void *data) {
  auto *state = listenerOwner<ToplevelState>(listener);
  auto *event = static_cast<wlr_xdg_toplevel_resize_event *>(data);
  if (state && event && event->seat && event->seat->seat == state->impl->seat &&
      event->edges)
    state->impl->beginClientWindowPointer(state->client, event->serial,
                                      event->edges);
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
  detachListener(state->sceneContentDestroy);
  detachListener(state->setTitle);
  detachListener(state->setAppId);
  detachListener(state->setParent);
  detachListener(state->requestMinimize);
  detachListener(state->requestMaximize);
  detachListener(state->requestFullscreen);
  detachListener(state->requestMove);
  detachListener(state->requestResize);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  destroyLegacyForeignToplevel(state);
#endif
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  destroyForeignToplevel(state);
  if (state->imageCaptureScene) {
    // Destroying the scene node tears down the scene-backed source and any
    // resources bound to it; wlroots 0.20 has no public source *_finish API.
    wlr_scene_node_destroy(&state->imageCaptureScene->tree.node);
    state->imageCaptureScene = nullptr;
    state->imageCaptureTree = nullptr;
    state->imageCaptureSource = nullptr;
  }
#endif
  client->nativeState = nullptr;
  if (client->sceneTree)
    wlr_scene_node_destroy(&client->sceneTree->node);
  client->sceneTree = nullptr;
  client->sceneContent = nullptr;
  if (client->surface)
    client->surface->data = nullptr;
  self->q->removeClient(client);
  delete state;
}

#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
void WaylandCompositor::Impl::handleForeignToplevelCaptureRequest(
    wl_listener *listener, void *data) {
  auto *self = listenerOwner<Impl>(listener);
  auto *request = static_cast<
      wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *>(data);
  if (!self || !request || !request->toplevel_handle)
    return;
  auto *state =
      static_cast<ToplevelState *>(request->toplevel_handle->data);
  if (!state || state->impl != self || !state->client ||
      !state->client->mapped || !state->imageCaptureScene)
    return;
  if (!state->imageCaptureSource) {
    state->imageCaptureSource =
        wlr_ext_image_capture_source_v1_create_with_scene_node(
            &state->imageCaptureScene->tree.node, self->eventLoop,
            self->allocator, self->renderer);
  }
  if (state->imageCaptureSource)
    wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
        request, state->imageCaptureSource);
}
#endif

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
  state->configured = false;
  state->impl->arrangeLayers();
  state->impl->q->arrange();
  state->impl->restoreLayerFocus();
}

void WaylandCompositor::Impl::handleLayerCommit(wl_listener *listener, void *) {
  auto *state = listenerOwner<LayerState>(listener);
  if (!state || !state->surface || !state->surface->initialized)
    return;
  const auto &current = state->surface->current;
  // Buffer-only commits (including every animated shell frame) do not change
  // the work area. Reconfiguring all windows here caused continuous relayout.
  if (!state->configured ||
      (current.committed &
       ~WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY)) {
    wlr_scene_tree *parent = state->impl->topLayer;
    switch (current.layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
      parent = state->impl->backgroundLayer;
      break;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
      parent = state->impl->bottomLayer;
      break;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
      parent = state->impl->overlayLayer;
      break;
    default:
      break;
    }
    wlr_scene_node_reparent(&state->sceneLayer->tree->node, parent);
    state->impl->arrangeLayers();
    state->impl->q->arrange();
    state->configured = true;
  }
  if (state->mapped &&
      (current.committed & WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY))
    state->impl->restoreLayerFocus();
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
  self->restoreLayerFocus();
}
void WaylandCompositor::Impl::restoreLayerFocus() {
  if (q->shuttingDown_ || q->testStopping_)
    return;
  LayerState *target = nullptr;
  for (auto *layer : layers) {
    if (!layer->mapped || !layer->surface ||
        layer->surface->current.keyboard_interactive !=
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
      continue;
    if (!target ||
        layer->surface->current.layer >= target->surface->current.layer)
      target = layer;
  }
  if (target)
    focusSurface(target->surface->surface);
  else if (q->focused_ && q->focused_->mapped && !q->focused_->minimized &&
           q->focused_->wlSurface)
    focusSurface(q->focused_->wlSurface);
  else if (seat)
    wlr_seat_keyboard_notify_clear_focus(seat);
}

ClientWindow *
WaylandCompositor::Impl::clientForSurface(wlr_surface *surface) const {
  if (!surface)
    return nullptr;
  surface = wlr_surface_get_root_surface(surface);
  auto *xdg = wlr_xdg_surface_try_from_wlr_surface(surface);
  while (xdg && xdg->role == WLR_XDG_SURFACE_ROLE_POPUP && xdg->popup &&
         xdg->popup->parent) {
    surface = wlr_surface_get_root_surface(xdg->popup->parent);
    xdg = wlr_xdg_surface_try_from_wlr_surface(surface);
  }
  for (const auto &client : q->clients_) {
    if (client->wlSurface == surface)
      return client.get();
  }
  return nullptr;
}

} // namespace LunaDash
