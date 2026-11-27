#include "compositor/client/ClientSurface.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/animation/AnimationProfile.hpp"
#include "compositor/xwayland/WindowIcon.hpp"

#include <algorithm>
#include <iterator>
#include <vector>
#include <unistd.h>

namespace LunaDash {

ClientWindow *WaylandCompositor::clientById(int id) const {
  const auto found =
      std::find_if(clients_.begin(), clients_.end(),
                   [id](const auto &client) { return client->id == id; });
  return found == clients_.end() ? nullptr : found->get();
}

void WaylandCompositor::updateClientMetadata(ClientWindow *client) {
  if (!readClientProtocolMetadata(client))
    return;
  setClientText(client->iconName,
                client->utility ? std::string()
                                : windowIconName(client->appId, client->title));
  // Taskbars see new titles, and X11 windows as they map.
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  if (d)
    d->updateClientLegacyForeignToplevel(client);
#endif
}

void WaylandCompositor::updateX11Icon(ClientWindow *client) {
  if (!client || !clientIsX11(*client) || !windowIcons_.isValid())
    return;
  if (*client->iconFile)
    ::unlink(client->iconFile);
  // A new name each time, so a picture cached by its path is not reused.
  const auto path = windowIcons_.filePath(
      std::to_string(client->id) + "-" +
      std::to_string(++client->iconRevision) + ".png");
  setClientText(client->iconFile,
                writeXWaylandIcon(client->surface.xwayland, path) ? path
                                                                  : std::string());
  scheduleEvents();
}

void WaylandCompositor::raiseWithDialogs(ClientWindow *client) {
  if (!client || !client->scene.tree)
    return;
  wlr_scene_node_raise_to_top(&client->scene.tree->node);
  for (const auto &dialog : clients_) {
    if (!dialog->state.floating || !dialog->scene.tree ||
        !dialog->scene.tree->node.enabled ||
        (!clientToplevel(*dialog) && !clientXWayland(*dialog)))
      continue;
    if (clientIsTransientOf(dialog.get(), client, clients_.size()))
      wlr_scene_node_raise_to_top(&dialog->scene.tree->node);
  }
}

void WaylandCompositor::focus(ClientWindow *client) {
  if (!d || !client || !client->state.mapped || client->state.minimized ||
      client->utility || client->state.workspace != workspace_ ||
      !clientReady(client))
    return;

  ClientWindow *previous = focused_;
  if (focused_ && focused_ != client && clientReady(focused_))
    setClientActivated(focused_, false);

  const bool changed = focused_ != client;
  focused_ = client;

  if (!client->state.fullscreen && client->state.floating &&
      client->layout.hiddenByMaximize) {
    for (const auto &other : clients_)
      if (!other->state.floating &&
          other->state.workspace == client->state.workspace &&
          other->state.maximized)
        setMaximized(other.get(), false);
    arrange();
  }
  if (!client->state.fullscreen && windowTemplate_ &&
      !windowTemplate_->allowOverlap && !client->state.floating &&
      !client->state.maximized &&
      windowLayout_->snapshot(client->state.workspace).maximizedWindow)
    setMaximized(client, true);
  if (!client->state.floating)
    windowLayout_->focus(client->id);
  // Selecting a tiled window must also scroll its column into view. Avoid
  // relayout for repeated clicks in the same window (including popup grabs).
  if (changed && (!client->state.floating || client->state.fullscreen ||
                  (previous && previous->state.fullscreen)))
    arrange();
  if (client->scene.tree)
    raiseWithDialogs(client);
  if (changed && windowAnimations_ && client->scene.tree)
    windowAnimations_->focus(client->scene.tree,
                             animationRectangle(client->layout.geometry));
  if (changed)
    setClientActivated(client, true);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  if (changed) {
    d->updateClientLegacyForeignToplevel(previous);
    d->updateClientLegacyForeignToplevel(client);
  }
#else
  (void)previous;
#endif
  d->focusSurface(clientSurface(*client));
  publishWindowLayout();
}

void WaylandCompositor::focusNext(int direction) {
  std::vector<ClientWindow *> visible;
  for (const auto &client : clients_)
    if (client->state.mapped && !client->state.minimized && !client->utility &&
        client->state.workspace == workspace_)
      visible.push_back(client.get());
  if (visible.empty()) {
    ClientWindow *previous = focused_;
    if (previous && clientReady(previous))
      setClientActivated(previous, false);
    focused_ = nullptr;
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
    if (d)
      d->updateClientLegacyForeignToplevel(previous);
#endif
    if (d && d->seat)
      wlr_seat_keyboard_notify_clear_focus(d->seat);
    return;
  }
  const auto found = std::find(visible.begin(), visible.end(), focused_);
  const auto index =
      found == visible.end() ? std::ptrdiff_t{-1}
                             : std::distance(visible.begin(), found);
  const auto count = static_cast<std::ptrdiff_t>(visible.size());
  focus(visible[static_cast<std::size_t>((index + direction + count) % count)]);
}

void WaylandCompositor::setMaximized(ClientWindow *client, bool maximized) {
  if (!client || client->state.maximized == maximized)
    return;
  if (!client->state.floating) {
    windowLayout_->setMaximized(client->id, maximized);
  }
  client->state.maximized = maximized;
  setClientMaximizedHint(client, maximized);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  if (d)
    d->updateClientLegacyForeignToplevel(client);
#endif
}

void WaylandCompositor::setFullscreen(ClientWindow *client, bool fullscreen) {
  if (!d || !client || client->state.fullscreen == fullscreen)
    return;

  if (fullscreen) {
    for (const auto &other : clients_) {
      if (other.get() == client ||
          other->state.workspace != client->state.workspace ||
          !other->state.fullscreen)
        continue;
      other->state.fullscreen = false;
      setClientFullscreenHint(other.get(), false);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
      d->updateClientLegacyForeignToplevel(other.get());
#endif
    }
  }

  client->state.fullscreen = fullscreen;
  setClientFullscreenHint(client, fullscreen);
#if LUDASH_WLR_HAS_FOREIGN_TOPLEVEL_MANAGEMENT
  d->updateClientLegacyForeignToplevel(client);
#endif

  arrange();
  if (fullscreen)
    focus(client);
}

void WaylandCompositor::minimizeClient(ClientWindow *client) {
  if (!client)
    return;
  client->state.minimized = true;
  setClientMinimizedHint(client, true);
  windowLayout_->setMinimized(client->id, true);
}

void WaylandCompositor::synchronizeWindowFocus() {
  const auto target = windowLayout_->snapshot(workspace_).focusedWindow;
  if (target) {
    auto *client = clientById(static_cast<int>(target));
    if (client && client->state.mapped && !client->state.minimized) {
      focus(client);
      return;
    }
  }
  focusNext(1);
}

void WaylandCompositor::removeClient(ClientWindow *client) {
  if (!client)
    return;
  if (d && d->foreignToplevels)
    d->foreignToplevels->remove(client->id);
  if (focused_ == client)
    focused_ = nullptr;
  windowSwitcher_->remove(client->id);
  windowLayout_->remove(client->id);
  std::erase_if(clients_,
                [client](const auto &entry) { return entry.get() == client; });
  arrange();
  synchronizeWindowFocus();
  if (lifecycle_.logoutPending && clients_.empty())
    terminateEventLoop(hasProcessFailure() ? 2 : 0);
}

} // namespace LunaDash
