#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/client/ClientSurface.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/animation/WindowAnimation.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/log/StallTrace.hpp"
#include "core/text/Text.hpp"
#include "desktop/wallpaper/VideoWallpaperPlayer.hpp"
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
  if (foreignHandle || !client->state.mapped || !impl->foreignToplevelList)
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
    client->scene.capture = nullptr;
    imageCaptureSource = nullptr;
  }
#endif
}

void WaylandCompositor::Impl::configureInitialToplevel(ClientWindow *client) {
  auto *xdg = client ? clientXdg(*client) : nullptr;
  if (!xdg || !xdg->toplevel || !xdg->initialized)
    return;
  auto *toplevel = xdg->toplevel;
  q->updateClientMetadata(client);
  q->arrange();
  client->state.fullscreen = toplevel->requested.fullscreen;
  client->state.maximized = toplevel->requested.maximized;
  if (!client->layout.initialRuleApplied && !client->utility &&
      !client->state.floating) {
    const auto rule = initialWindowRule(
        *q->pluginManager_,
        {{"appId", client->appId},
         {"title", client->title},
         {"id", client->id}},
        client->state.workspace, client->state.maximized, q->workspaceCount());
    client->state.workspace = jsonInt(rule, "workspace");
    client->state.maximized = jsonBool(rule, "maximized");
  }
  client->layout.initialRuleApplied = true;
  client->layout.lastSize = {};
  client->layout.pending =
      q->windowTemplate_ && !q->windowTemplate_->allowOverlap &&
      !client->state.floating && !client->utility &&
      !client->state.fullscreen && !client->state.maximized;
  if (client->layout.pending) {
    q->arrange();
    if (client->layout.lastSize.isValid())
      return;
  } else {
    // A pre-map maximize/fullscreen request can replace a reserved tile.
    q->arrange();
  }
  const Size size =
      client->state.fullscreen ? outputSize() : q->workArea().size();
  const bool constrained = client->state.fullscreen || client->state.maximized;
  wlr_xdg_toplevel_set_size(toplevel,
                            constrained ? std::max(1, size.width()) : 0,
                            constrained ? std::max(1, size.height()) : 0);
  wlr_xdg_toplevel_set_maximized(toplevel, client->state.maximized);
  wlr_xdg_toplevel_set_fullscreen(toplevel, client->state.fullscreen);
}

void WaylandCompositor::Impl::addXdgToplevel(wlr_xdg_surface *surface,
                                             wlr_xdg_toplevel *toplevel) {
  if (!surface || !toplevel)
    return;

  auto client = std::make_unique<ClientWindow>();
  client->id = q->nextWindowId_++;
  client->surface.xdg = surface;
  client->state.workspace = q->workspace_;
  client->scene.tree = wlr_scene_tree_create(normalLayer);
  if (!client->scene.tree)
    return;
  client->scene.content =
      wlr_scene_xdg_surface_create(client->scene.tree, surface);
  if (!client->scene.content) {
    wlr_scene_node_destroy(&client->scene.tree->node);
    return;
  }

  client->scene.tree->node.data = client.get();
  surface->data = client->scene.tree;
  if (surface->client && surface->client->client) {
    pid_t pid = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    wl_client_get_credentials(surface->client->client, &pid, &uid, &gid);
    client->processId = static_cast<std::int64_t>(pid);
  }

  auto *current = client.get();
  auto *state = new ToplevelState(this, current);
  q->clients_.push_back(std::move(client));
  q->updateClientMetadata(current);
  state->listenTo(surface, toplevel);
  wlr_scene_node_set_enabled(&current->scene.tree->node, false);
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
  listen<&ToplevelState::onSceneContentDestroy,
         &ToplevelState::sceneContentDestroy>(
      &client->scene.content->node.events.destroy);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  // Window capture gets a private scene so the stream does not inherit
  // workspace transforms, tiling clips or visibility of the desktop scene.
  imageCaptureScene = wlr_scene_create();
  if (imageCaptureScene) {
    client->scene.capture = wlr_scene_xdg_surface_create(
        &imageCaptureScene->tree, client->surface.xdg);
    if (!client->scene.capture) {
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
  listen<&ToplevelState::onMap, &ToplevelState::map>(&surfaceEvents.map);
  // Capture the last mapped frame before wlroots disables the subsurface
  // tree. The surface still owns its buffer at the start of the unmap signal.
  listenFirst<&ToplevelState::onUnmap, &ToplevelState::unmap>(
      &surfaceEvents.unmap);
  listen<&ToplevelState::onCommit, &ToplevelState::commit>(
      &surfaceEvents.commit);
  listen<&ToplevelState::onDestroy, &ToplevelState::destroy>(
      WlrootsCompat::xdgToplevelDestroySignal(surface, toplevel));

  auto &events = toplevel->events;
  listen<&ToplevelState::onMetadata, &ToplevelState::setTitle>(
      &events.set_title);
  listen<&ToplevelState::onMetadata, &ToplevelState::setAppId>(
      &events.set_app_id);
  listen<&ToplevelState::onParent, &ToplevelState::setParent>(
      &events.set_parent);
  listen<&ToplevelState::onMinimize, &ToplevelState::requestMinimize>(
      &events.request_minimize);
  listen<&ToplevelState::onMaximize, &ToplevelState::requestMaximize>(
      &events.request_maximize);
  listen<&ToplevelState::onFullscreen, &ToplevelState::requestFullscreen>(
      &events.request_fullscreen);
  listen<&ToplevelState::onMove, &ToplevelState::requestMove>(
      &events.request_move);
  listen<&ToplevelState::onResize, &ToplevelState::requestResize>(
      &events.request_resize);
}

void WaylandCompositor::Impl::ToplevelState::onMap() {
  StallTrace::Section trace("window map", client ? client->appId : std::string());
  auto *desktop = impl->q;
  client->state.mapped = true;
  impl->finishSplash();
  client->layout.pending = false;
  desktop->updateClientMetadata(client);
  impl->adoptShellDialog(client);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  createForeignToplevel();
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  createLegacyForeignToplevel();
#endif
  // A new ordinary window joins the visible layout without inheriting zoom.
  desktop->arrange();
  if (!client->utility)
    desktop->focus(client);
  // The window fades in to its steady opacity (what the glass gives it every
  // frame), not to opaque and then a sudden step down when the fade ends.
  if (client->scene.tree && !client->state.fullscreen && !client->utility) {
    float opacity = impl->eyeCareActive ? 1.0f : impl->steadyWindowOpacity;
    wlr_scene_node_for_each_buffer(
        &client->scene.tree->node,
        [](wlr_scene_buffer *buffer, int, int, void *data) {
          wlr_scene_buffer_set_opacity(buffer, *static_cast<float *>(data));
        },
        &opacity);
  }
  if (desktop->windowAnimations_ && client->scene.tree && !client->utility)
    desktop->windowAnimations_->open(
        client->scene.tree,
        SceneAnimationRectangle{
            client->layout.geometry.x(), client->layout.geometry.y(),
            client->layout.geometry.width(), client->layout.geometry.height()});
}

void WaylandCompositor::Impl::ToplevelState::onUnmap() {
  StallTrace::Section trace("window unmap");
  auto *desktop = impl->q;
  if (desktop->windowAnimations_ && client->scene.tree) {
    desktop->windowAnimations_->close(client->scene.tree, impl->animationLayer);
    desktop->windowAnimations_->cancel(client->scene.tree);
  }
  if (impl->seat->keyboard_state.focused_surface ==
      client->surface.xdg->surface)
    wlr_seat_keyboard_notify_clear_focus(impl->seat);
#if LUDASH_WLR_HAS_EXT_WINDOW_CAPTURE
  destroyForeignToplevel();
#endif
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  destroyLegacyForeignToplevel();
#endif
  client->state.mapped = false;
  client->layout.pending = false;
  client->layout.initialRuleApplied = false;
  client->layout.preferredFloatingSize = {};
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
  auto *xdg = client->surface.xdg;
  if (!xdg->toplevel)
    return;
  StallTrace::Section trace("window commit", client->appId);
  wlr_box geometry{};
#if WLR_VERSION_MINOR >= 19
  geometry = xdg->geometry;
#else
  wlr_xdg_surface_get_geometry(xdg, &geometry);
#endif
  const bool geometryChanged =
      !hasSurfaceGeometry || lastSurfaceGeometry.x != geometry.x ||
      lastSurfaceGeometry.y != geometry.y ||
      lastSurfaceGeometry.width != geometry.width ||
      lastSurfaceGeometry.height != geometry.height;
  lastSurfaceGeometry = geometry;
  hasSurfaceGeometry = true;

  if (xdg->initial_commit) {
    // Requests such as set_maximized can arrive before the first surface
    // commit. Apply them only after wlroots marks the role initialized.
    impl->configureInitialToplevel(client);
    return;
  }

  // Content-only commits (video, browser animation, terminal cursor) do not
  // change clipping geometry. Rebuild rounded views only when the xdg surface
  // geometry itself changes.
  if (geometryChanged && client->state.mapped)
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
  if (client->state.floating)
    impl->q->windowLayout_->remove(client->id);
  impl->q->arrange();
}

void WaylandCompositor::Impl::ToplevelState::onMinimize() {
  if (impl->q->refusesMinimize(*client, true))
    return;
  client->state.minimized = true;
  impl->q->windowLayout_->setMinimized(client->id, true);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  updateLegacyForeignToplevel();
#endif
  impl->q->arrange();
  impl->q->synchronizeWindowFocus();
}

void WaylandCompositor::Impl::ToplevelState::onMaximize() {
  auto *xdg = client->surface.xdg;
  if (!xdg->initialized)
    return;
  if (!client->state.mapped) {
    impl->configureInitialToplevel(client);
    return;
  }
  impl->q->setMaximized(client, xdg->toplevel->requested.maximized);
  impl->q->arrange();
  if (client->state.maximized)
    impl->q->focus(client);
}

void WaylandCompositor::Impl::ToplevelState::onFullscreen() {
  auto *xdg = client->surface.xdg;
  if (!xdg->initialized)
    return;
  if (!client->state.mapped) {
    impl->configureInitialToplevel(client);
    return;
  }
  impl->q->setFullscreen(client, xdg->toplevel->requested.fullscreen);
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
  client->scene.content = nullptr;
}

void WaylandCompositor::Impl::ToplevelState::onDestroy() {
  StallTrace::Section trace("window destroy");
  Impl *owner = impl;
  ClientWindow *window = client;
  // Disconnects every listener and releases the protocol objects first.
  delete this;
  if (window->scene.tree)
    wlr_scene_node_destroy(&window->scene.tree->node);
  window->scene.tree = nullptr;
  window->scene.content = nullptr;
  window->surface.xdg->data = nullptr;
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
  listen<&LayerState::onMap, &LayerState::map>(&surface->surface->events.map);
  listen<&LayerState::onUnmap, &LayerState::unmap>(
      &surface->surface->events.unmap);
  // The first surface commit marks the role initialized; onCommit performs the
  // initial configure and layout at that point.
  listen<&LayerState::onCommit, &LayerState::commit>(
      &surface->surface->events.commit);
  listen<&LayerState::onDestroy, &LayerState::destroy>(
      &surface->events.destroy);
}

void WaylandCompositor::Impl::LayerState::onMap() {
  StallTrace::Section trace("layer map", surface && surface->namespace_ ? surface->namespace_ : "");
  mapped = true;
  if (surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND)
    impl->finishSplash();
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
  impl->releaseShellDialogs();
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

bool WaylandCompositor::Impl::shellOverlayHasKeyboard() const {
  return std::any_of(layers.begin(), layers.end(), [](const LayerState *layer) {
    return layer->mapped && layer->surface &&
           layer->surface->current.keyboard_interactive ==
               ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
  });
}

void WaylandCompositor::Impl::adoptShellDialog(ClientWindow *client) {
  if (!client || client->utility || !shellOverlayHasKeyboard())
    return;
  client->shellDialog = true;
  client->state.floating = true;
}

void WaylandCompositor::Impl::releaseShellDialogs() {
  if (shellOverlayHasKeyboard())
    return;
  for (const auto &client : q->clients_)
    if (client->shellDialog) {
      client->shellDialog = false;
      client->state.floating = false;
    }
}

void WaylandCompositor::Impl::restoreLayerFocus() {
  if (q->lifecycle_.shuttingDown || q->lifecycle_.testStopping)
    return;
  // A dialog the shell opened keeps the keyboard over the overlay that opened it.
  for (auto it = q->clients_.rbegin(); it != q->clients_.rend(); ++it)
    if ((*it)->shellDialog && (*it)->state.mapped && clientSurface(**it)) {
      focusSurface(clientSurface(**it));
      return;
    }
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
  else if (q->focused_ && q->focused_->state.mapped &&
           !q->focused_->state.minimized && clientSurface(*q->focused_))
    focusSurface(clientSurface(*q->focused_));
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
    if (clientSurface(*client) == surface)
      return client.get();
  }
  return nullptr;
}

} // namespace LunaDash
