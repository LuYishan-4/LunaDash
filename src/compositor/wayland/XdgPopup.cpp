#include "compositor/wayland/Register.hpp"

namespace LunaDash {
using Templates::attachListener;
using Templates::detachListener;
using Templates::listenerOwner;

wlr_scene_tree *
WaylandCompositor::Impl::sceneForSurface(wlr_surface *surface) const {
  if (!surface)
    return nullptr;
  if (auto *xdg = wlr_xdg_surface_try_from_wlr_surface(surface))
    return static_cast<wlr_scene_tree *>(xdg->data);
  for (const auto *layer : layers)
    if (layer->surface->surface == surface)
      return layer->sceneLayer->tree;
  return nullptr;
}

void WaylandCompositor::Impl::configureXdgPopup(XdgPopupState *state) {
  auto *popup = state->popup;
  if (!popup->base->initialized || !popup->parent)
    return;
  if (!state->sceneTree) {
    auto *parent = sceneForSurface(popup->parent);
    if (!parent)
      return;
    state->sceneTree = wlr_scene_xdg_surface_create(parent, popup->base);
    popup->base->data = state->sceneTree;
    if (!state->sceneTree)
      return;
  }
  // Positioner constraints are expressed relative to the root parent, even
  // when this is a submenu of another popup. Allow menus outside the window
  // while keeping their requested flip/slide/resize behavior within the output.
  auto *root = popup->parent;
  auto *xdg = wlr_xdg_surface_try_from_wlr_surface(root);
  while (xdg && xdg->role == WLR_XDG_SURFACE_ROLE_POPUP && xdg->popup->parent) {
    root = xdg->popup->parent;
    xdg = wlr_xdg_surface_try_from_wlr_surface(root);
  }
  if (auto *tree = sceneForSurface(root)) {
    int x = 0, y = 0;
    wlr_scene_node_coords(&tree->node, &x, &y);
    const auto size = outputSize();
    const wlr_box bounds{-x, -y, size.width(), size.height()};
    wlr_xdg_popup_unconstrain_from_box(popup, &bounds);
  }
  wlr_xdg_surface_schedule_configure(popup->base);
}

void WaylandCompositor::Impl::addXdgPopup(wlr_xdg_popup *popup) {
  if (!popup || !popup->base)
    return;
  auto *state = new XdgPopupState;
  state->impl = this;
  state->popup = popup;
  xdgPopups.append(state);
  attachListener(&popup->base->surface->events.commit, state->commit, state,
                 handleXdgPopupCommit);
  attachListener(&popup->events.reposition, state->reposition, state,
                 handleXdgPopupReposition);
#if WLR_VERSION_MINOR < 18
  attachListener(&popup->base->events.destroy, state->destroy, state,
                 handleXdgPopupDestroy);
  // 0.17 announces the role during its first commit, before this listener.
  if (popup->base->initial_commit)
    configureXdgPopup(state);
#else
  attachListener(&popup->events.destroy, state->destroy, state,
                 handleXdgPopupDestroy);
#endif
}

void WaylandCompositor::Impl::handleNewXdgPopup(wl_listener *listener,
                                                void *data) {
  if (auto *self = listenerOwner<Impl>(listener))
    self->addXdgPopup(static_cast<wlr_xdg_popup *>(data));
}

void WaylandCompositor::Impl::handleXdgPopupCommit(wl_listener *listener,
                                                   void *) {
  auto *state = listenerOwner<XdgPopupState>(listener);
  if (state && state->popup->base->initial_commit)
    state->impl->configureXdgPopup(state);
}

void WaylandCompositor::Impl::handleXdgPopupReposition(wl_listener *listener,
                                                       void *) {
  auto *state = listenerOwner<XdgPopupState>(listener);
  if (state)
    state->impl->configureXdgPopup(state);
}

void WaylandCompositor::Impl::handleXdgPopupDestroy(wl_listener *listener,
                                                    void *) {
  auto *state = listenerOwner<XdgPopupState>(listener);
  if (!state)
    return;
  detachListener(state->commit);
  detachListener(state->reposition);
  detachListener(state->destroy);
  // The scene helper owns its surface/node listeners and destroys the tree.
  state->popup->base->data = nullptr;
  state->impl->xdgPopups.removeAll(state);
  delete state;
}
} // namespace LunaDash
