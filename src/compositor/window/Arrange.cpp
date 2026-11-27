#include "compositor/client/ClientSurface.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/layout/FreeformLayout.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/WindowTuning.hpp"
#include "compositor/window/animation/AnimationProfile.hpp"
#include "compositor/renderer/color/HexColor.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "desktop/theme/ThemeSync.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/desktop/PreferenceKeys.hpp"
#include "core/json/JsonAccess.hpp"
#include "core/log/StallTrace.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {


template <typename Clients>
bool belongsToWindow(const ClientWindow &client, const ClientWindow *owner,
                     const Clients &clients) {
  if (!owner)
    return false;
  return &client == owner ||
         clientIsTransientOf(&client, owner, clients.size());
}

} // namespace

void WaylandCompositor::applyVisualPreferences(const Json &preferences) {
  // Stacking outlines: the accent around the focused window, a quiet line
  // around the others.
  const auto palette = appearancePalette(preferences);
  const auto premultiplied = [](const std::string &hex, float alpha, float *rgba) {
    if (!parseHexColor(hex, rgba))
      return;
    for (int channel = 0; channel < 3; ++channel)
      rgba[channel] *= alpha;
    rgba[3] = alpha;
  };
  premultiplied(jsonString(palette, "accent"), 0.95f, d->borderFocused);
  premultiplied(jsonString(palette, "border"), 0.6f, d->borderUnfocused);
  d->eyeCareActive = preferenceValue(preferences, Preference::eyeCare);
  const bool blur =
      preferenceValue(preferences, Preference::blur) && !d->eyeCareActive;
  d->steadyWindowOpacity =
      d->eyeCareActive
          ? 1.0f
          : preferenceValue(preferences, Preference::windowOpacity) / 100.0f;
  // Keep the backdrop visible through clients that render fully opaque buffers.
  if (blur)
    d->steadyWindowOpacity = std::min(d->steadyWindowOpacity, 0.84f);
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
  // Leaving stacking, windows remember their places for when it comes back.
  if (windowTemplate_ && windowTemplate_->allowOverlap)
    for (const auto &client : clients_)
      for (const auto &slot : windowLayout_->snapshot(client->state.workspace).columns)
        if (slot.window == static_cast<LayoutWindowId>(client->id))
          client->layout.stackingGeometry = slot.geometry;
  windowTemplate_ = &selectedTemplate;
  // GTK applications draw their own titlebars: offer them every button a
  // stacking desktop has.
  synchronizeWindowButtons(windowTemplate_->allowOverlap);
  windowLayout_ = createWindowLayout(*windowTemplate_);
  installWindowPlacementFilter();
  return true;
}

// Switching to stacking, each window goes back to where it was the last time
// stacking was in use; the others are placed like new windows, at the default
// size where they cover the rest least. No window's size depends on another's.
void WaylandCompositor::placeSwitchedToStacking() {
  for (const auto &client : clients_) {
    const auto &place = client->layout.stackingGeometry;
    if (windowUsesManagedLayout(*client) && place.isValid())
      performLayoutAction("place", {{"window", client->id}, {"x", place.x()},
                                    {"y", place.y()}, {"width", place.width()},
                                    {"height", place.height()}});
  }
}

Rect WaylandCompositor::floatingGeometry(const ClientWindow &client,
                                         const Rect &area) const {
  if (client.state.maximized)
    return area;
  const Size preferred = client.layout.preferredFloatingSize.isValid()
                             ? client.layout.preferredFloatingSize
                             : WindowTuning::defaultFloatingSize;
  const Size bounded(std::min(preferred.width(), area.width()),
                     std::min(preferred.height(), area.height()));
  // A dialog opens centred over the window it belongs to.
  Rect over = area;
  for (const auto &candidate : clients_)
    if (clientIsTransientOf(&client, candidate.get(), 1) &&
        candidate->layout.geometry.isValid())
      over = candidate->layout.geometry;
  const Rect centered(over.x() + (over.width() - bounded.width()) / 2,
                      over.y() + (over.height() - bounded.height()) / 2,
                      bounded.width(), bounded.height());
  auto geometry = client.layout.manualGeometry.isValid()
                      ? client.layout.manualGeometry
                      : centered;
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
  if (!clientReady(client) || !client->scene.tree)
    return;
  const Rect previous = client->layout.geometry;
  client->layout.geometry = rectangle;
  // Only the window under the pointer follows it directly; everything else,
  // the windows it pushes aside included, moves on the shared spring.
  const bool dragged = d->pointerWindow == client->id;
  if (dragged && d->pointerFollowing) {
    // The pointer places it until it is dropped (finishWindowPointer).
  } else if (windowAnimations_)
    windowAnimations_->relayout(
        client->scene.tree, animationRectangle(previous),
        animationRectangle(rectangle), d->animationLayer,
        client->state.mapped && client->scene.tree->node.enabled &&
            !client->layout.manualResize && !dragged,
        d->styleSwitch ? &WindowTuning::styleSwitchSpring : nullptr);
  else
    wlr_scene_node_set_position(&client->scene.tree->node, rectangle.x(),
                                rectangle.y());
  configureClientSurface(client, rectangle);
  d->updateWindowBorder(client);
}


ClientWindow *WaylandCompositor::workspaceFullscreen() const {
  for (const auto &candidate : clients_)
    if (candidate->state.mapped && !candidate->state.minimized &&
        candidate->state.fullscreen &&
        candidate->state.workspace == workspace_ && !candidate->utility)
      return candidate.get();
  return nullptr;
}

bool WaylandCompositor::steppedAsideFor(const ClientWindow &fullscreen) const {
  return focused_ && focused_ != &fullscreen &&
         focused_->state.workspace == fullscreen.state.workspace &&
         focused_->state.mapped && !focused_->state.minimized &&
         !focused_->utility && !belongsToWindow(*focused_, &fullscreen, clients_);
}

Rect WaylandCompositor::columnGeometry(const ClientWindow &client) const {
  if (!client.state.fullscreen || client.state.floating)
    return client.layout.geometry;
  for (const auto &placement :
       windowLayout_->presentation(client.state.workspace, workArea()))
    if (static_cast<int>(placement.window) == client.id &&
        placement.geometry.isValid())
      return placement.geometry;
  return client.layout.geometry;
}

bool WaylandCompositor::refusesMinimize(const ClientWindow &client,
                                        bool minimize) const {
  return minimize && client.state.fullscreen;
}

void WaylandCompositor::arrange() {
  if (!d || !d->scene)
    return;
  StallTrace::Section trace("arrange");



  d->schedulePointerRebase();
  compactWorkspaces();


  const int slide = std::exchange(workspaceSlide_, 0);

  const bool inOverview = overviewShown();


  const int slideDistance =
      slide * (std::max(1, d->outputSize().height()) + WindowTuning::workspaceSlideGap);
  const auto preferences = desktopPreferences();
  applyVisualPreferences(preferences);
  const bool templateChanged = selectWindowTemplate();
  d->styleSwitch = templateChanged;

  for (const auto &client : clients_) {
    if (templateChanged)
      d->applyDecorationMode(client.get());
  }



  if (windowAnimations_)
    windowAnimations_->configure(sceneAnimationOptions(
        windowAnimationProfile(*pluginManager_, preferences, *windowTemplate_),
        windowTemplate_->motion));
  const int count = workspaceCount();
  workspace_ = std::clamp(workspace_, 0, std::max(0, count - 1));
  const Rect area = workArea();
  const Size output = d->outputSize();
  const Rect fullscreenArea(0, 0, std::max(1, output.width()),
                            std::max(1, output.height()));
  windowLayout_->configure(currentWindowLayoutSettings());

  ClientWindow *fullscreenClient = workspaceFullscreen();


  ClientWindow *steppedAside = fullscreenClient && steppedAsideFor(*fullscreenClient)
                                   ? std::exchange(fullscreenClient, nullptr)
                                   : nullptr;
  if (d->fullscreenBackdrop) {
    wlr_scene_rect_set_size(d->fullscreenBackdrop, fullscreenArea.width(),
                            fullscreenArea.height());
    wlr_scene_node_set_enabled(&d->fullscreenBackdrop->node,
                               fullscreenClient != nullptr);
  }

  // Keep the layout's window set in step with the clients.
  for (const auto &client : clients_) {
    client->state.workspace = std::min(client->state.workspace, count - 1);
    // A window's own size, as it first drew itself free (stacking, or a
    // floating dialog); a tile's size is the layout's, not its own.
    if (client->state.mapped && !client->layout.preferredFloatingSize.isValid() &&
        ((windowTemplate_->allowOverlap && !templateChanged) ||
         client->state.floating)) {
      const Size committed = clientCommittedSize(*client);
      if (committed.width() > 0 && committed.height() > 0)
        client->layout.preferredFloatingSize = committed;
    }
    if (!windowUsesManagedLayout(*client)) {
      windowLayout_->remove(client->id);
      continue;
    }


    const Size preferred =
        windowTemplate_ && windowTemplate_->allowOverlap &&
                client->layout.preferredFloatingSize.isValid()
            ? client->layout.preferredFloatingSize
            : Size();
    windowLayout_->insert(client->state.workspace, client->id, preferred);
    windowLayout_->moveToWorkspace(client->id, client->state.workspace);
    windowLayout_->setMinimized(client->id, client->state.minimized);
    if (client->state.maximized)
      windowLayout_->setMaximized(client->id, true);
  }

  if (templateChanged && windowTemplate_->allowOverlap)
    placeSwitchedToStacking();
  if (templateChanged && focused_ && focused_->state.mapped &&
      !focused_->state.minimized)
    windowLayout_->focus(focused_->id);


  std::vector<ClientWindow *> revealed;
  std::unordered_map<int, WorkspaceLayoutSnapshot> snapshots;
  for (const auto &client : clients_) {
    auto snapshotIt = snapshots.find(client->state.workspace);
    if (snapshotIt == snapshots.end())
      snapshotIt =
          snapshots
              .emplace(client->state.workspace,
                       windowLayout_->snapshot(client->state.workspace))
              .first;
    const auto maximized = snapshotIt->second.maximizedWindow;
    if (!client->state.floating &&
        (client->state.mapped || client->layout.pending)) {


      bool next = maximized == static_cast<LayoutWindowId>(client->id);
      for (const auto &placement : snapshotIt->second.columns)
        if (placement.window == static_cast<LayoutWindowId>(client->id))
          next = next || placement.maximized;
      if (client->state.maximized != next) {
        client->state.maximized = next;
        setClientMaximizedHint(client.get(), next);
      }
    }

    //fullscreen
    const bool inMaximizedFamily =
        belongsToWindow(*client, clientById(static_cast<int>(maximized)), clients_);
    client->layout.hiddenByMaximize = windowHiddenByMaximize(
        *windowTemplate_, maximized, inMaximizedFamily, *client);
    const bool inFullscreenFamily =
        belongsToWindow(*client, fullscreenClient, clients_);
    const bool besideFocus = belongsToWindow(*client, steppedAside, clients_);

    const bool unmanagedX11 = clientIsX11(*client) && client->utility;
    const bool visible =
        client->state.mapped && !client->state.minimized &&
        client->state.workspace == workspace_ && !besideFocus &&
        (unmanagedX11 ||
         (!client->utility &&
          (fullscreenClient ? inFullscreenFamily
                            : !client->layout.hiddenByMaximize)));
    if (client->scene.tree) {
      // Shell dialogs go above the shell's overlay, which takes the top of
      // the layers below.
      auto *targetLayer =
          (unmanagedX11 || client->shellDialog) && d->overlayLayer ? d->overlayLayer
          : fullscreenClient && inFullscreenFamily && d->fullscreenLayer
              ? d->fullscreenLayer
              : d->normalLayer;
      if (client->scene.tree->node.parent != targetLayer)
        wlr_scene_node_reparent(&client->scene.tree->node, targetLayer);
      const bool wasVisible = client->scene.tree->node.enabled;
      // A window leaving with the previous workspace stays enabled until its
      // slide settles; SceneAnimation hides it then. Arranging again meanwhile
      // (focus changes do) must not cut the slide short.
      const bool slidingOut = windowAnimations_ &&
                              windowAnimations_->slidingOut(client->scene.tree);
      bool keepSliding = !visible && slidingOut && client->state.mapped;
      if (visible && slidingOut)
        windowAnimations_->returnHome(client->scene.tree);
      if (!visible && wasVisible && !slidingOut && client->state.mapped &&
          windowAnimations_ && !inOverview) {
        if (slide && client->state.workspace != workspace_)
          keepSliding = windowAnimations_->slideOut(
              client->scene.tree, animationRectangle(client->layout.geometry),
              0, -slideDistance);
        else if (client->state.minimized)
          // Minimizing drops the window toward the dock.
          windowAnimations_->close(
              client->scene.tree, d->animationLayer, 0,
              std::max(WindowTuning::minimizeDropMinimum,
                       client->layout.geometry.height() /
                           WindowTuning::minimizeDropDivisor));
        else
          windowAnimations_->close(client->scene.tree, d->animationLayer);
      }
      if (visible && !wasVisible && client->layout.geometry.isValid())
        revealed.push_back(client.get());
      if (!visible && windowAnimations_ && !keepSliding)
        windowAnimations_->cancel(client->scene.tree);
      wlr_scene_node_set_enabled(&client->scene.tree->node,
                                 visible || keepSliding);
    }

    if (!visible)
      continue;
    if (fullscreenClient && client.get() == fullscreenClient) {
      configure(client.get(), fullscreenArea);
      continue;
    }
    if (client->state.floating)
      configure(client.get(), floatingGeometry(*client, area));
  }

  // Pending clients need their first size even when a rule sends them to a
  // different workspace, or an existing fullscreen window covers this one.
  std::vector<int> layoutWorkspaces{workspace_};
  for (const auto &client : clients_)
    if (client->layout.pending &&
        std::find(layoutWorkspaces.begin(), layoutWorkspaces.end(),
                  client->state.workspace) == layoutWorkspaces.end())
      layoutWorkspaces.push_back(client->state.workspace);
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
    if (!client || client == steppedAside || client->state.minimized ||
        placement.hiddenByMaximize ||
        (!client->layout.pending &&
         (fullscreenClient || client->state.workspace != workspace_)))
      continue;
    configure(client, placement.geometry);
  }

  // Switching between tiling and stacking, windows that come out from behind
  // a maximized one slide from their old place (configure) instead.
  if (windowAnimations_ && !inOverview && !templateChanged)
    for (auto *client : revealed) {
      if (slide)
        windowAnimations_->open(client->scene.tree,
                                animationRectangle(client->layout.geometry), 0,
                                slideDistance, false);
      else
        windowAnimations_->open(client->scene.tree,
                                animationRectangle(client->layout.geometry));
    }

  if (focused_ && focused_->scene.tree && focused_->state.mapped &&
      !focused_->state.minimized && focused_->state.workspace == workspace_)
    raiseWithDialogs(focused_);

  d->styleSwitch = false;
  d->updateWindowCorners();
  publishWindowLayout();
  publishWorkspaces();
  scheduleEvents();
}

void WaylandCompositor::publishWorkspaces() {
  if (!d || !d->workspaceProtocol)
    return;
  std::vector<wlr_output *> outputs;
  outputs.reserve(d->outputs.size());
  for (const auto *output : d->outputs)
    if (output && output->output)
      outputs.push_back(output->output);
  d->workspaceProtocol->update(workspaceCount(), workspace_, outputs);
}

} // namespace LunaDash
