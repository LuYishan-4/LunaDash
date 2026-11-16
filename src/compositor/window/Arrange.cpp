#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/WindowTuning.hpp"
#include "compositor/window/animation/AnimationProfile.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/desktop/PreferenceKeys.hpp"
#include "core/json/JsonAccess.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {

// The size a client asked for in its first commit, used as its floating size.
Size committedClientSize(const ClientWindow &client) {
#if LUDASH_WLR_HAS_XWAYLAND
  if (client.x11 && client.xwayland)
    return Size(client.xwayland->width, client.xwayland->height);
#endif
  if (!client.surface)
    return Size();
  wlr_box geometry{};
#if WLR_VERSION_MINOR >= 19
  geometry = client.surface->geometry;
#else
  wlr_xdg_surface_get_geometry(client.surface, &geometry);
#endif
  return Size(geometry.width, geometry.height);
}

// True when `client` is `owner` or one of its (transient) dialogs.
template <typename Clients>
bool belongsToWindow(const ClientWindow &client, const ClientWindow *owner,
                     const Clients &clients) {
  if (!owner)
    return false;
  if (&client == owner)
    return true;
  auto *parent = client.toplevel ? client.toplevel->parent : nullptr;
  for (std::size_t depth = 0; parent && depth < clients.size(); ++depth) {
    if (owner->toplevel && parent == owner->toplevel)
      return true;
    parent = parent->parent;
  }
  return false;
}

} // namespace

void WaylandCompositor::applyVisualPreferences(const Json &preferences) {
  d->eyeCareActive = preferenceValue(preferences, Preference::eyeCare);
  d->steadyWindowOpacity =
      d->eyeCareActive
          ? 1.0f
          : preferenceValue(preferences, Preference::windowOpacity) / 100.0f;
  const bool blur =
      preferenceValue(preferences, Preference::blur) && !d->eyeCareActive;
  // Tell ext-background-effect-v1 clients whether the blur they ask for is applied.
  if (d->backgroundEffects)
    d->backgroundEffects->setBlurAvailable(blur);
  if (d->windowGlass) {
    const bool changed = d->windowGlass->configure(
        blur, preferenceValue(preferences, Preference::blurRadius),
        d->steadyWindowOpacity);
    if (changed)
      for (const auto *output : d->outputs)
        wlr_output_schedule_frame(output->output);
  }
}

void WaylandCompositor::installWindowPlacementFilter() {
  windowLayout_->setPlacementFilter(
      [this](auto workspace, auto area, const auto &placements) {
        return pluginWindowPlacements(*pluginManager_, *windowTemplate_,
                                      workspace, area, placements);
      });
}

bool WaylandCompositor::selectWindowTemplate() {
  const auto &selectedTemplate = windowTemplateForKey(selectedWindowTemplateKey());
  if (windowTemplate_ && windowTemplate_->key == selectedTemplate.key)
    return false;
  if (d->pointerWindow)
    d->finishWindowPointer(false);
  windowTemplate_ = &selectedTemplate;
  windowLayout_ = createWindowLayout(*windowTemplate_);
  installWindowPlacementFilter();
  return true;
}

Rect WaylandCompositor::floatingGeometry(const ClientWindow &client,
                                         const Rect &area) const {
  if (client.maximized)
    return area;
  const Size preferred = client.preferredFloatingSize.isValid()
                             ? client.preferredFloatingSize
                             : WindowTuning::defaultFloatingSize;
  const Size bounded(std::min(preferred.width(), area.width()),
                     std::min(preferred.height(), area.height()));
  const Rect centered(area.x() + (area.width() - bounded.width()) / 2,
                      area.y() + (area.height() - bounded.height()) / 2,
                      bounded.width(), bounded.height());
  auto geometry =
      client.manualGeometry.isValid() ? client.manualGeometry : centered;
  geometry.setSize(geometry.size().boundedTo(area.size()));
  geometry.moveLeft(std::clamp(geometry.x(), area.left(),
                               area.right() - geometry.width() + 1));
  geometry.moveTop(std::clamp(geometry.y(), area.top(),
                              area.bottom() - geometry.height() + 1));
  return geometry;
}

Rect WaylandCompositor::workArea() const {
  Rect area = d ? d->usableArea : Rect();
  if (!d) {
    const Size size = virtualOutputSize(desktopPreferences());
    area = Rect(0, 0, size.width(), size.height());
  }
  const auto settings = currentWindowLayoutSettings();
  const int gap = jsonContains(settings, "gap")
                      ? jsonInt(settings, "gap")
                      : preferenceValue(desktopPreferences(), Preference::gap);
  area.adjust(gap, gap, -gap, -gap);
  if (area.width() < 1)
    area.setWidth(1);
  if (area.height() < 1)
    area.setHeight(1);
  return area;
}

void WaylandCompositor::configure(ClientWindow *client,
                                  const Rect &rectangle) {
  if (!clientReady(client) || !client->sceneTree)
    return;
  if (windowAnimations_)
    windowAnimations_->relayout(
        client->sceneTree, animationRectangle(client->geometry),
        animationRectangle(rectangle), d->animationLayer,
        client->mapped && client->sceneTree->node.enabled &&
            !client->manualResize && !d->pointerWindow);
  else
    wlr_scene_node_set_position(&client->sceneTree->node, rectangle.x(),
                                rectangle.y());
  client->geometry = rectangle;
  const Size size(std::max(1, rectangle.width()),
                  std::max(1, rectangle.height()));

#if LUDASH_WLR_HAS_XWAYLAND
  if (client->x11 && client->xwayland) {
    const auto x =
        static_cast<int16_t>(std::clamp(rectangle.x(), -32768, 32767));
    const auto y =
        static_cast<int16_t>(std::clamp(rectangle.y(), -32768, 32767));
    const auto width =
        static_cast<uint16_t>(std::clamp(size.width(), 1, 65535));
    const auto height =
        static_cast<uint16_t>(std::clamp(size.height(), 1, 65535));
    const auto *surface = client->xwayland;
    if (surface->x != x || surface->y != y || surface->width != width ||
        surface->height != height)
      wlr_xwayland_surface_configure(client->xwayland, x, y, width, height);
    if (surface->maximized_horz != client->maximized ||
        surface->maximized_vert != client->maximized)
      WlrootsCompat::setXWaylandMaximized(client->xwayland, client->maximized);
    if (surface->fullscreen != client->fullscreen)
      wlr_xwayland_surface_set_fullscreen(client->xwayland, client->fullscreen);
    client->lastSize = size;
    return;
  }
#endif

  wlr_box geometry{};
#if WLR_VERSION_MINOR >= 19
  geometry = client->surface->geometry;
#else
  wlr_xdg_surface_get_geometry(client->surface, &geometry);
#endif
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
  // Tell tiled clients that the size is not a suggestion. Without the tiled state a
  // toolkit may replace it with its own preferred size (winit does, so Slint windows
  // drew at their preferred size inside a larger tile).
  const std::uint32_t tiled =
      client->floating || client->maximized || client->fullscreen
          ? 0
          : WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT;
  if (client->toplevel->scheduled.tiled != tiled)
    wlr_xdg_toplevel_set_tiled(client->toplevel, tiled);
}

void WaylandCompositor::arrange() {
  if (!d || !d->scene)
    return;
  // Windows and layers move or change here: the surface under the cursor may differ.
  d->schedulePointerRebase();
  compactWorkspaces();
  // A workspace change slides the whole old workspace out and the new one in.
  const int slide = std::exchange(workspaceSlide_, 0);
  const int slideDistance =
      slide * (std::max(1, d->outputSize().width()) + WindowTuning::workspaceSlideGap);
  const auto preferences = desktopPreferences();
  applyVisualPreferences(preferences);
  const bool templateChanged = selectWindowTemplate();
  if (windowAnimations_)
    windowAnimations_->configure(sceneAnimationOptions(windowAnimationProfile(
        *pluginManager_, preferences, *windowTemplate_)));
  const int count = workspaceCount();
  workspace_ = std::clamp(workspace_, 0, std::max(0, count - 1));
  const Rect area = workArea();
  const Size output = d->outputSize();
  const Rect fullscreenArea(0, 0, std::max(1, output.width()),
                            std::max(1, output.height()));
  windowLayout_->configure(currentWindowLayoutSettings());

  ClientWindow *fullscreenClient = nullptr;
  for (const auto &candidate : clients_) {
    if (candidate->mapped && !candidate->minimized && candidate->fullscreen &&
        candidate->workspace == workspace_ && !candidate->utility) {
      fullscreenClient = candidate.get();
      break;
    }
  }

  // Keep the layout's window set in step with the clients.
  for (const auto &client : clients_) {
    client->workspace = std::min(client->workspace, count - 1);
    if (client->mapped && !client->preferredFloatingSize.isValid()) {
      const Size committed = committedClientSize(*client);
      if (committed.width() > 0 && committed.height() > 0)
        client->preferredFloatingSize = committed;
    }
    if (!windowUsesManagedLayout(*client)) {
      windowLayout_->remove(client->id);
      continue;
    }
    const Size preferred = windowTemplate_ && windowTemplate_->allowOverlap
                               ? client->preferredFloatingSize
                               : Size();
    windowLayout_->insert(client->workspace, client->id, preferred);
    windowLayout_->moveToWorkspace(client->id, client->workspace);
    windowLayout_->setMinimized(client->id, client->minimized);
    if (client->maximized)
      windowLayout_->setMaximized(client->id, true);
  }

  if (templateChanged && focused_ && focused_->mapped && !focused_->minimized)
    windowLayout_->focus(focused_->id);

  // Visibility, scene layers and floating geometry.
  std::vector<ClientWindow *> revealed;
  std::unordered_map<int, LayoutWindowId> maximizedWindows;
  for (const auto &client : clients_) {
    auto maximizedIt = maximizedWindows.find(client->workspace);
    if (maximizedIt == maximizedWindows.end())
      maximizedIt = maximizedWindows
                        .emplace(client->workspace,
                                 windowLayout_->snapshot(client->workspace)
                                     .maximizedWindow)
                        .first;
    const auto maximized = maximizedIt->second;
    if (!client->floating && (client->mapped || client->layoutPending))
      client->maximized = maximized == static_cast<LayoutWindowId>(client->id);
    const bool inMaximizedFamily =
        belongsToWindow(*client, clientById(static_cast<int>(maximized)), clients_);
    client->hiddenByMaximize = windowHiddenByMaximize(
        *windowTemplate_, maximized, inMaximizedFamily, *client);
    const bool inFullscreenFamily =
        belongsToWindow(*client, fullscreenClient, clients_);

    const bool unmanagedX11 = client->x11 && client->utility;
    const bool visible =
        client->mapped && !client->minimized &&
        client->workspace == workspace_ &&
        (unmanagedX11 ||
         (!client->utility &&
          (fullscreenClient ? inFullscreenFamily : !client->hiddenByMaximize)));
    if (client->sceneTree) {
      auto *targetLayer =
          unmanagedX11 && d->overlayLayer ? d->overlayLayer
          : fullscreenClient && inFullscreenFamily && d->fullscreenLayer
              ? d->fullscreenLayer
              : d->normalLayer;
      if (client->sceneTree->node.parent != targetLayer)
        wlr_scene_node_reparent(&client->sceneTree->node, targetLayer);
      const bool wasVisible = client->sceneTree->node.enabled;
      if (!visible && wasVisible && client->mapped && windowAnimations_) {
        if (slide && client->workspace != workspace_)
          windowAnimations_->close(client->sceneTree, d->animationLayer,
                                   -slideDistance, 0, false);
        else if (client->minimized)
          // Minimizing drops the window toward the dock.
          windowAnimations_->close(
              client->sceneTree, d->animationLayer, 0,
              std::max(WindowTuning::minimizeDropMinimum,
                       client->geometry.height() / WindowTuning::minimizeDropDivisor));
        else
          windowAnimations_->close(client->sceneTree, d->animationLayer);
      }
      if (visible && !wasVisible && client->geometry.isValid())
        revealed.push_back(client.get());
      if (!visible && windowAnimations_)
        windowAnimations_->cancel(client->sceneTree);
      wlr_scene_node_set_enabled(&client->sceneTree->node, visible);
    }

    if (!visible)
      continue;
    if (fullscreenClient && client.get() == fullscreenClient) {
      configure(client.get(), fullscreenArea);
      continue;
    }
    if (client->floating)
      configure(client.get(), floatingGeometry(*client, area));
  }

  // Pending clients need their first size even when a rule sends them to a
  // different workspace, or an existing fullscreen window covers this one.
  std::vector<int> layoutWorkspaces{workspace_};
  for (const auto &client : clients_)
    if (client->layoutPending &&
        std::find(layoutWorkspaces.begin(), layoutWorkspaces.end(),
                  client->workspace) == layoutWorkspaces.end())
      layoutWorkspaces.push_back(client->workspace);
  std::vector<WindowPlacement> placements;
  for (const int workspace : layoutWorkspaces)
    for (auto &placement : windowLayout_->presentation(workspace, area))
      placements.push_back(std::move(placement));
  if (windowTemplate_->key != selectedWindowTemplateKey()) {
    arrange();
    return;
  }
  for (const auto &placement : placements) {
    auto *client = clientById(static_cast<int>(placement.window));
    if (!client || client->minimized || placement.hiddenByMaximize ||
        (!client->layoutPending &&
         (fullscreenClient || client->workspace != workspace_)))
      continue;
    configure(client, placement.geometry);
  }

  if (windowAnimations_)
    for (auto *client : revealed) {
      if (slide)
        windowAnimations_->open(client->sceneTree,
                                animationRectangle(client->geometry),
                                slideDistance, 0, false);
      else
        windowAnimations_->open(client->sceneTree,
                                animationRectangle(client->geometry));
    }

  if (focused_ && focused_->sceneTree && focused_->mapped &&
      !focused_->minimized && focused_->workspace == workspace_)
    raiseWithDialogs(focused_);

  d->updateBackground();
  d->updateWindowCorners();
  publishWindowLayout();
}

} // namespace LunaDash
