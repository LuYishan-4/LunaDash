#include "compositor/window/animation/WindowAnimation.hpp"
#include "core/log/StallTrace.hpp"
#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "desktop/wallpaper/VideoWallpaperPlayer.hpp"
#include "core/text/Text.hpp"
#include <algorithm>

namespace LunaDash {

namespace {
    struct LayerSlideOffset {
      int dx = 0;
      int dy = 0;
    };


    LayerSlideOffset layerSlide(const wlr_layer_surface_v1 *surface) {
      constexpr int distance = 18;
      const auto anchor = surface->current.anchor;
      const bool top = anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
      const bool bottom = anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
      const bool left = anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
      const bool right = anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
      if (top != bottom)
        return {0, top ? -distance : distance};
      if (left != right)
        return {left ? -distance : distance, 0};
      return {};
    }
} // namespace

SceneAnimationRectangle
WaylandCompositor::Impl::layerRectangle(const LayerState *state) {
  const auto *tree = state->sceneLayer->tree;
  return {tree->node.x, tree->node.y,
          static_cast<int>(state->surface->current.actual_width),
          static_cast<int>(state->surface->current.actual_height)};
}

bool WaylandCompositor::Impl::animatesLayer(const LayerState *state) const {
  return q->windowAnimations_ && state && state->surface && state->sceneLayer &&
         state->surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND &&
         !(state->surface->namespace_ &&
           std::string_view(state->surface->namespace_) == "selection") &&
         !(q->screenCapture_ && q->screenCapture_->busy());
}

std::string safeUtf8(const char *text) {
  // Keep metadata from clients valid UTF-8 for JSON state and IPC.
  return text ? sanitizedUtf8(text) : std::string();
}


#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
void WaylandCompositor::Impl::ToplevelState::updateForeignToplevel() {
  if (!foreignHandle)
    return;
  const std::string title = client->title;
  const std::string appId = client->appId;
  const wlr_ext_foreign_toplevel_handle_v1_state foreignState{
      .title = title.c_str(),
      .app_id = appId.c_str(),
  };
  wlr_ext_foreign_toplevel_handle_v1_update_state(foreignHandle, &foreignState);
}

void WaylandCompositor::Impl::ToplevelState::createForeignToplevel() {
  if (foreignHandle || !client->mapped || !impl->foreignToplevelList)
    return;
  const std::string title = client->title;
  const std::string appId = client->appId;
  const wlr_ext_foreign_toplevel_handle_v1_state foreignState{
      .title = title.c_str(),
      .app_id = appId.c_str(),
  };
  foreignHandle = wlr_ext_foreign_toplevel_handle_v1_create(
      impl->foreignToplevelList, &foreignState);
  if (foreignHandle)
    foreignHandle->data = this;
}

void WaylandCompositor::Impl::ToplevelState::destroyForeignToplevel() {
  if (!foreignHandle)
    return;
  foreignHandle->data = nullptr;
  wlr_ext_foreign_toplevel_handle_v1_destroy(foreignHandle);
  foreignHandle = nullptr;
}
#endif

#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
void WaylandCompositor::Impl::updateClientLegacyForeignToplevel(
    ClientWindow *client) {
  // Every window, Wayland or X11 (ForeignToplevels).
  if (client && foreignToplevels)
    foreignToplevels->update(*client, q->focused_ == client);
}

void WaylandCompositor::Impl::ToplevelState::updateLegacyForeignToplevel() {
  impl->updateClientLegacyForeignToplevel(client);
}

void WaylandCompositor::Impl::ToplevelState::createLegacyForeignToplevel() {
  impl->updateClientLegacyForeignToplevel(client);
}

void WaylandCompositor::Impl::ToplevelState::destroyLegacyForeignToplevel() {
  if (impl->foreignToplevels)
    impl->foreignToplevels->remove(client->id);
}
#endif

void WaylandCompositor::Impl::ToplevelState::releaseProtocolObjects() {
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  destroyLegacyForeignToplevel();
#endif
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  destroyForeignToplevel();
  if (imageCaptureScene) {
    // Destroying the scene node tears down the scene-backed source and any
    // resources bound to it; wlroots 0.20 has no public source *_finish API.
    wlr_scene_node_destroy(&imageCaptureScene->tree.node);
    imageCaptureScene = nullptr;
    imageCaptureTree = nullptr;
    imageCaptureSource = nullptr;
  }
#endif
}

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
        client->workspace, client->maximized, q->workspaceCount());
    client->workspace = jsonInt(rule, "workspace");
    client->maximized = jsonBool(rule, "maximized");
  }
  client->initialRuleApplied = true;
  client->lastSize = {};
  client->layoutPending = q->windowTemplate_ &&
                          !q->windowTemplate_->allowOverlap &&
                          !client->floating && !client->utility &&
                          !client->fullscreen && !client->maximized;
  if (client->layoutPending) {
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
  const Size size = client->fullscreen ? outputSize() : q->workArea().size();
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
    client->processId = static_cast<std::int64_t>(pid);
  }

  auto *current = client.get();
  auto *state = new ToplevelState(this, current);
  current->nativeState = state;
  q->clients_.push_back(std::move(client));
  q->updateClientMetadata(current);
  state->listenTo(surface, toplevel);
  wlr_scene_node_set_enabled(&current->sceneTree->node, false);
#if WLR_VERSION_MINOR < 18
  // wlroots 0.17 emits xdg_shell.new_surface from the role's first commit.
  // The commit listener is installed too late for that commit, so configure
  // immediately while initial_commit is still set.
  if (surface->initial_commit)
    configureInitialToplevel(current);
#endif
}

WaylandCompositor::Impl::ToplevelState::ToplevelState(Impl *owner,
                                                      ClientWindow *window)
    : impl(owner), client(window) {
  listen<&ToplevelState::onSceneContentDestroy>(
      &client->sceneContent->node.events.destroy, sceneContentDestroy);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  // Window capture gets a private scene so the stream does not inherit
  // workspace transforms, tiling clips or visibility of the desktop scene.
  imageCaptureScene = wlr_scene_create();
  if (imageCaptureScene) {
    imageCaptureTree =
        wlr_scene_xdg_surface_create(&imageCaptureScene->tree, client->surface);
    if (!imageCaptureTree) {
      wlr_scene_node_destroy(&imageCaptureScene->tree.node);
      imageCaptureScene = nullptr;
    }
  }
#endif
}

WaylandCompositor::Impl::ToplevelState::~ToplevelState() {
  releaseProtocolObjects();
}

void WaylandCompositor::Impl::ToplevelState::listenTo(
    wlr_xdg_surface *surface, wlr_xdg_toplevel *toplevel) {
  auto &surfaceEvents = surface->surface->events;
  listen<&ToplevelState::onMap>(&surfaceEvents.map, map);
  // Capture the last mapped frame before wlroots disables the subsurface
  // tree. The surface still owns its buffer at the start of the unmap signal.
  listenFirst<&ToplevelState::onUnmap>(&surfaceEvents.unmap, unmap);
  listen<&ToplevelState::onCommit>(&surfaceEvents.commit, commit);
  listen<&ToplevelState::onDestroy>(
      WlrootsCompat::xdgToplevelDestroySignal(surface, toplevel), destroy);

  auto &events = toplevel->events;
  listen<&ToplevelState::onMetadata>(&events.set_title, setTitle);
  listen<&ToplevelState::onMetadata>(&events.set_app_id, setAppId);
  listen<&ToplevelState::onParent>(&events.set_parent, setParent);
  listen<&ToplevelState::onMinimize>(&events.request_minimize, requestMinimize);
  listen<&ToplevelState::onMaximize>(&events.request_maximize, requestMaximize);
  listen<&ToplevelState::onFullscreen>(&events.request_fullscreen,
                                       requestFullscreen);
  listen<&ToplevelState::onMove>(&events.request_move, requestMove);
  listen<&ToplevelState::onResize>(&events.request_resize, requestResize);
}

void WaylandCompositor::Impl::ToplevelState::onMap() {
  StallTrace::Section trace("window map", client ? client->appId : std::string());
  auto *desktop = impl->q;
  client->mapped = true;
  client->layoutPending = false;
  desktop->updateClientMetadata(client);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  // Only mapped windows belong in the portal's selectable toplevel list.
  createForeignToplevel();
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  createLegacyForeignToplevel();
#endif
  // A new ordinary window joins the visible layout instead of inheriting zoom.
  if (desktop->windowTemplate_ && !desktop->windowTemplate_->allowOverlap &&
      !client->floating && !client->utility)
    for (const auto &peer : desktop->clients_)
      if (peer.get() != client && peer->workspace == client->workspace &&
          !peer->floating && peer->maximized)
        desktop->setMaximized(peer.get(), false);
  desktop->arrange();
  if (!client->utility)
    desktop->focus(client);
  if (desktop->windowAnimations_ && client->sceneTree && !client->utility)
    desktop->windowAnimations_->open(
        client->sceneTree,
        SceneAnimationRectangle{client->geometry.x(), client->geometry.y(),
                                client->geometry.width(),
                                client->geometry.height()});
}

void WaylandCompositor::Impl::ToplevelState::onUnmap() {
  StallTrace::Section trace("window unmap");
  auto *desktop = impl->q;
  if (desktop->windowAnimations_ && client->sceneTree) {
    desktop->windowAnimations_->close(client->sceneTree, impl->animationLayer);
    desktop->windowAnimations_->cancel(client->sceneTree);
  }
  if (impl->seat->keyboard_state.focused_surface == client->wlSurface)
    wlr_seat_keyboard_notify_clear_focus(impl->seat);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  destroyForeignToplevel();
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  destroyLegacyForeignToplevel();
#endif
  client->mapped = false;
  client->layoutPending = false;
  client->initialRuleApplied = false;
  client->preferredFloatingSize = {};
  desktop->windowSwitcher_->remove(client->id);
  if (impl->pointerWindow == client->id)
    impl->finishWindowPointer(false);
  desktop->windowLayout_->remove(client->id);
  if (desktop->focused_ == client)
    desktop->focused_ = nullptr;
  desktop->arrange();
  desktop->synchronizeWindowFocus();
}

void WaylandCompositor::Impl::ToplevelState::onCommit() {
  if (!client->surface || !client->toplevel)
    return;
  StallTrace::Section trace("window commit", client->appId);
  wlr_box geometry{};
#if WLR_VERSION_MINOR >= 19
  geometry = client->surface->geometry;
#else
  wlr_xdg_surface_get_geometry(client->surface, &geometry);
#endif
  const bool geometryChanged =
      !hasSurfaceGeometry || lastSurfaceGeometry.x != geometry.x ||
      lastSurfaceGeometry.y != geometry.y ||
      lastSurfaceGeometry.width != geometry.width ||
      lastSurfaceGeometry.height != geometry.height;
  lastSurfaceGeometry = geometry;
  hasSurfaceGeometry = true;

  if (client->surface->initial_commit) {
    // Requests such as set_maximized can arrive before the first surface
    // commit. Apply them only after wlroots marks the role initialized.
    impl->configureInitialToplevel(client);
    return;
  }

  // Content-only commits (video, browser animation, terminal cursor) do not
  // change clipping geometry. Rebuild rounded views only when the xdg surface
  // geometry itself changes.
  if (geometryChanged && client->mapped)
    impl->updateWindowCorners();
}

void WaylandCompositor::Impl::ToplevelState::onMetadata() {
  impl->q->updateClientMetadata(client);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  updateForeignToplevel();
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  updateLegacyForeignToplevel();
#endif
  impl->q->arrange();
}

void WaylandCompositor::Impl::ToplevelState::onParent() {
  impl->q->updateClientMetadata(client);
  if (client->floating)
    impl->q->windowLayout_->remove(client->id);
  impl->q->arrange();
}

void WaylandCompositor::Impl::ToplevelState::onMinimize() {
  client->minimized = true;
  impl->q->windowLayout_->setMinimized(client->id, true);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  updateLegacyForeignToplevel();
#endif
  impl->q->arrange();
  impl->q->synchronizeWindowFocus();
}

void WaylandCompositor::Impl::ToplevelState::onMaximize() {
  if (!client->surface || !client->surface->initialized)
    return;
  if (!client->mapped) {
    impl->configureInitialToplevel(client);
    return;
  }
  impl->q->setMaximized(client, client->toplevel->requested.maximized);
  impl->q->arrange();
  if (client->maximized)
    impl->q->focus(client);
}

void WaylandCompositor::Impl::ToplevelState::onFullscreen() {
  if (!client->surface || !client->surface->initialized)
    return;
  if (!client->mapped) {
    impl->configureInitialToplevel(client);
    return;
  }
  impl->q->setFullscreen(client, client->toplevel->requested.fullscreen);
}

void WaylandCompositor::Impl::ToplevelState::onMove(
    wlr_xdg_toplevel_move_event *event) {
  if (event && event->seat && event->seat->seat == impl->seat)
    impl->beginClientWindowPointer(client, event->serial, 0);
}

void WaylandCompositor::Impl::ToplevelState::onResize(
    wlr_xdg_toplevel_resize_event *event) {
  if (event && event->seat && event->seat->seat == impl->seat && event->edges)
    impl->beginClientWindowPointer(client, event->serial, event->edges);
}

void WaylandCompositor::Impl::ToplevelState::onSceneContentDestroy() {
  Templates::detachListener(sceneContentDestroy);
  client->sceneContent = nullptr;
}

void WaylandCompositor::Impl::ToplevelState::onDestroy() {
  StallTrace::Section trace("window destroy");
  Impl *owner = impl;
  ClientWindow *window = client;
  window->nativeState = nullptr;
  // Disconnects every listener and releases the protocol objects first.
  delete this;
  if (window->sceneTree)
    wlr_scene_node_destroy(&window->sceneTree->node);
  window->sceneTree = nullptr;
  window->sceneContent = nullptr;
  if (window->surface)
    window->surface->data = nullptr;
  owner->q->removeClient(window);
}

bool WaylandCompositor::videoWallpaperSurface(
    const wlr_layer_surface_v1 *surface) const {
  const pid_t player = videoWallpaper_ ? videoWallpaper_->processId() : -1;
  if (player <= 0 || !surface || !surface->resource)
    return false;
  pid_t pid = 0;
  wl_client_get_credentials(wl_resource_get_client(surface->resource), &pid,
                            nullptr, nullptr);
  return pid == player;
}


WaylandCompositor::Impl::LayerState::LayerState(
    Impl *owner, wlr_layer_surface_v1 *layerSurface,
    wlr_scene_layer_surface_v1 *scene)
    : impl(owner), surface(layerSurface), sceneLayer(scene) {
  listen<&LayerState::onMap>(&surface->surface->events.map, map);
  listen<&LayerState::onUnmap>(&surface->surface->events.unmap, unmap);
  // The first surface commit marks the role initialized; onCommit performs the
  // initial configure and layout at that point.
  listen<&LayerState::onCommit>(&surface->surface->events.commit, commit);
  listen<&LayerState::onDestroy>(&surface->events.destroy, destroy);
}

void WaylandCompositor::Impl::LayerState::onMap() {
  StallTrace::Section trace("layer map", surface && surface->namespace_ ? surface->namespace_ : "");
  mapped = true;
  impl->arrangeLayers();
  impl->q->arrange();
  if (impl->animatesLayer(this)) {
    const auto [dx, dy] = layerSlide(surface);
    impl->q->windowAnimations_->open(sceneLayer->tree, layerRectangle(this), dx,
                                     dy);
  }
  if (surface->current.keyboard_interactive !=
      ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
    impl->focusSurface(surface->surface);
}

void WaylandCompositor::Impl::LayerState::onUnmap() {
  StallTrace::Section trace("layer unmap");
  if (impl->animatesLayer(this)) {
    // wlroots disables the layer's tree before this listener runs; the snapshot needs its
    // buffers, which stay attached until the surface commit completes.
    auto *tree = sceneLayer->tree;
    const bool enabled = tree->node.enabled;
    wlr_scene_node_set_enabled(&tree->node, true);
    const auto [dx, dy] = layerSlide(surface);
    impl->q->windowAnimations_->close(tree, tree->node.parent, dx, dy);
    wlr_scene_node_set_enabled(&tree->node, enabled);
    impl->q->windowAnimations_->cancel(tree);
  }
  mapped = false;
  configured = false;
  impl->arrangeLayers();
  impl->q->arrange();
  impl->restoreLayerFocus();
}

void WaylandCompositor::Impl::LayerState::onCommit() {
  if (!surface->initialized)
    return;
  StallTrace::Section trace("layer commit", surface->namespace_ ? surface->namespace_ : "");
  const auto &current = surface->current;
  // Buffer-only commits (including every animated shell frame) do not change
  // the work area. Reconfiguring all windows here caused continuous relayout.
  if (!configured ||
      (current.committed & ~WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY)) {
    wlr_scene_tree *parent = impl->topLayer;
    switch (current.layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
      parent = impl->backgroundLayer;
      break;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
      parent = impl->bottomLayer;
      break;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
      parent = impl->overlayLayer;
      break;
    default:
      break;
    }
    wlr_scene_node_reparent(&sceneLayer->tree->node, parent);
    // Reparenting raises the surface. The video wallpaper plays beneath the
    // shell's (then transparent) wallpaper and its desktop clock.
    if (impl->q->videoWallpaperSurface(surface)) {
      // Just above the compositor's neutral fallback colour.
      if (impl->background && impl->background->node.parent == parent)
        wlr_scene_node_place_above(&sceneLayer->tree->node,
                                   &impl->background->node);
      else
        wlr_scene_node_lower_to_bottom(&sceneLayer->tree->node);
    }
    impl->arrangeLayers();
    impl->q->arrange();
    configured = true;
  }
  if (mapped &&
      (current.committed & WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY))
    impl->restoreLayerFocus();
}

void WaylandCompositor::Impl::LayerState::onDestroy() {
  StallTrace::Section trace("layer destroy");
  Impl *owner = impl;
  std::erase(owner->layers, this);
  delete this;
  owner->arrangeLayers();
  owner->q->arrange();
  owner->restoreLayerFocus();
}

void WaylandCompositor::Impl::restoreLayerFocus() {
  if (q->lifecycle_.shuttingDown || q->lifecycle_.testStopping)
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
