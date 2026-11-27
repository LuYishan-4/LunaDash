#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/layout/FreeformLayout.hpp"
#include "compositor/renderer/color/HexColor.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/animation/AnimationProfile.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTuning.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/json/JsonAccess.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <linux/input-event-codes.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace LunaDash {
namespace {

std::uint64_t monotonicNow() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<std::uint64_t>(now.tv_sec) * 1000000000ULL +
         static_cast<std::uint64_t>(now.tv_nsec);
}



// scene_node_for_each_scene_buffer
void scene_node_dfs(wlr_scene_node *node, wlr_scene_tree *parent, double x, double y,
                  double scale, bool root, float opacity,
                  std::unordered_map<wlr_scene_buffer *, wlr_scene_buffer *> &copies,
                  std::unordered_set<wlr_scene_buffer *> &seen) {
  if (!root && !node->enabled)
    return;
  if (!root) {
    x += node->x;
    y += node->y;
  }
  if (node->type == WLR_SCENE_NODE_TREE) {
    auto *tree = wlr_scene_tree_from_node(node);
    wlr_scene_node *child = nullptr;
    wl_list_for_each(child, &tree->children, link)
        scene_node_dfs(child, parent, x, y, scale, false, opacity, copies, seen);
    return;
  }
  if (node->type != WLR_SCENE_NODE_BUFFER)
    return;
  auto *buffer = wlr_scene_buffer_from_node(node);
  if (!buffer->buffer)
    return;
  int width = buffer->dst_width;
  int height = buffer->dst_height;
  if (width <= 0 || height <= 0) {
    width = buffer->buffer->width;
    height = buffer->buffer->height;
    if (buffer->transform % 2)
      std::swap(width, height);
  }
  auto existing = copies.find(buffer);
  wlr_scene_buffer *copy = existing == copies.end() ? nullptr : existing->second;
  if (!copy) {
    copy = wlr_scene_buffer_create(parent, buffer->buffer);
    if (!copy)
      return;
    copy->point_accepts_input = [](wlr_scene_buffer *, double *, double *) {
      return false;
    };
    copies.emplace(buffer, copy);
  }
  seen.insert(buffer);
  wlr_scene_buffer_set_source_box(copy, &buffer->src_box);
  wlr_scene_buffer_set_transform(copy, buffer->transform);
  wlr_scene_buffer_set_buffer(copy, buffer->buffer);
  wlr_scene_buffer_set_dest_size(
      copy, std::max(1, static_cast<int>(std::lround(width * scale))),
      std::max(1, static_cast<int>(std::lround(height * scale))));
  wlr_scene_buffer_set_opacity(copy, opacity >= 0.0f ? opacity : buffer->opacity);
  wlr_scene_node_set_position(&copy->node, static_cast<int>(std::lround(x * scale)),
                              static_cast<int>(std::lround(y * scale)));
}

void updateFrame(wlr_scene_tree *parent, wlr_scene_tree *&frame, const Rect &rect,
                 int width, const float color[4]) {
  if (!frame && width > 0) {
    frame = wlr_scene_tree_create(parent);
    for (int edge = 0; frame && edge < 4; ++edge)
      wlr_scene_rect_create(frame, 1, 1, color);
  }
  if (!frame)
    return;
  wlr_scene_node_set_enabled(&frame->node, width > 0);
  if (width <= 0)
    return;
  wlr_scene_node_set_position(&frame->node, rect.x() - width, rect.y() - width);
  const int outerWidth = rect.width() + 2 * width;
  const int outerHeight = rect.height() + 2 * width;
  // Top, bottom, left, right.
  const std::array<Rect, 4> edges{
      Rect(0, 0, outerWidth, width), Rect(0, outerHeight - width, outerWidth, width),
      Rect(0, width, width, rect.height()),
      Rect(outerWidth - width, width, width, rect.height())};
  std::size_t index = 0;
  wlr_scene_node *child = nullptr;
  wl_list_for_each(child, &frame->children, link) {
    const Rect &box = edges[index++];
    auto *edge = wlr_scene_rect_from_node(child);
    wlr_scene_rect_set_size(edge, box.width(), box.height());
    wlr_scene_rect_set_color(edge, color);
    wlr_scene_node_set_position(child, box.x(), box.y());
  }
}

// Stacking windows spread out so none hides another: each scaled down (never
// up) into its cell of a grid over `area`.
std::vector<Rect> spreadOut(const std::vector<Rect> &windows, const Rect &area) {
  auto places = windowGrid(windows, area, 24);
  for (std::size_t index = 0; index < windows.size(); ++index) {
    const Rect &window = windows[index];
    Rect &cell = places[index];
    const double fit =
        std::min({1.0, static_cast<double>(cell.width()) / std::max(1, window.width()),
                  static_cast<double>(cell.height()) / std::max(1, window.height())});
    Rect fitted(Point(), Size(std::max(1, static_cast<int>(std::lround(window.width() * fit))),
                              std::max(1, static_cast<int>(std::lround(window.height() * fit)))));
    fitted.moveCenter(cell.center());
    cell = fitted;
  }
  return places;
}

Rect interpolated(const Rect &from, const Rect &to, double progress) {
  const auto mix = [progress](int a, int b) {
    return static_cast<int>(std::lround(a + (b - a) * progress));
  };
  return Rect(mix(from.x(), to.x()), mix(from.y(), to.y()),
              std::max(1, mix(from.width(), to.width())),
              std::max(1, mix(from.height(), to.height())));
}

} // namespace

bool WaylandCompositor::overviewShown() const {
  return overview_.open || overview_.progress > 0.0 || overview_.tree;
}

bool WaylandCompositor::overviewOpen() const { return overview_.open; }

void WaylandCompositor::toggleOverview() {
  if (overview_.open) {
    closeOverview();
    return;
  }
  if (!d || !d->overviewLayer)
    return;
  if (windowSwitcher_->active())
    finishWindowSwitch(false);
  if (d->pointerWindow)
    d->finishWindowPointer(false);
  const auto preferences = desktopPreferences();
  parseHexColor(jsonString(preferences, "accent"), overview_.accent);
  overview_.open = true;

  wlr_seat_pointer_notify_clear_focus(d->seat);
  overview_.lastStep = monotonicNow();
  overview_.view = workspace_;
  overview_.viewVelocity = 0.0;
  for (const auto *output : d->outputs)
    wlr_output_schedule_frame(output->output);
}

void WaylandCompositor::closeOverview() {
  overview_.open = false;
  overview_.pressed = 0;
  overview_.dragging = false;
  overview_.hovered = 0;
  overview_.scroll = 0.0;
  overview_.scrollHorizontal = 0.0;
  if (!d)
    return;
  for (const auto *output : d->outputs)
    wlr_output_schedule_frame(output->output);

  d->schedulePointerRebase();
}

Rect WaylandCompositor::overviewRect(const Rect &rectangle, int workspace) const {
  const Size output = d->outputSize();
  const double scale = 1.0 + (WindowTuning::overviewZoom - 1.0) * overview_.progress;
  const double centerX = output.width() / 2.0;
  const double centerY = output.height() / 2.0;
  const double offset = (static_cast<double>(workspace) - overview_.view) *
                        (output.height() + WindowTuning::overviewWorkspaceGap);
  const double left = centerX + (rectangle.x() - centerX) * scale;
  const double top = centerY + (rectangle.y() + offset - centerY) * scale;
  return Rect(static_cast<int>(std::lround(left)), static_cast<int>(std::lround(top)),
              std::max(1, static_cast<int>(std::lround(rectangle.width() * scale))),
              std::max(1, static_cast<int>(std::lround(rectangle.height() * scale))));
}

const WaylandCompositor::OverviewState::Preview *
WaylandCompositor::overviewPreviewAt(double x, double y, int except) const {
  // The last drawn is on top.
  for (auto it = overview_.previews.rbegin(); it != overview_.previews.rend(); ++it)
    if (it->window != except && x >= it->rect.left() && x <= it->rect.right() &&
        y >= it->rect.top() && y <= it->rect.bottom())
      return &*it;
  return nullptr;
}

int WaylandCompositor::overviewWorkspaceAt(double y) const {
  const Size output = d->outputSize();
  const double scale = 1.0 + (WindowTuning::overviewZoom - 1.0) * overview_.progress;
  const double centerY = output.height() / 2.0;
  const double unscaled = centerY + (y - centerY) / std::max(0.01, scale);
  const double band = output.height() + WindowTuning::overviewWorkspaceGap;
  // Measured from the workspace drawn in the middle, which may be sliding.
  const double origin = (overview_.view - workspace_) * band;
  const int workspace =
      workspace_ + static_cast<int>(std::floor((unscaled + origin) / band));
  return std::clamp(workspace, 0, std::max(0, workspaceCount() - 1));
}

void WaylandCompositor::rebuildOverviewPreviews() {
  if (!overview_.tree) {
    if (!overviewShown() || !d || !d->overviewLayer) {
      overview_.previews.clear();
      return;
    }
    overview_.tree = wlr_scene_tree_create(d->overviewLayer);
    if (!overview_.tree)
      return;
  }
  if (!overviewShown()) {
    wlr_scene_node_destroy(&overview_.tree->node);
    overview_.tree = nullptr;
    overview_.dim = nullptr;
    overview_.previews.clear();
    overview_.previewTrees.clear();
    overview_.previewFrames.clear();
    overview_.previewBuffers.clear();
    overview_.tileMotion.clear();
    return;
  }

  const Size output = d->outputSize();
  // Dim the wallpaper so the windows stand out.
  const float dim[4] = {0.0f, 0.0f, 0.0f,
                        static_cast<float>(0.35 * std::clamp(overview_.progress, 0.0, 1.0))};
  if (!overview_.dim)
    overview_.dim = wlr_scene_rect_create(overview_.tree, output.width(),
                                          output.height(), dim);
  else {
    wlr_scene_rect_set_size(overview_.dim, output.width(), output.height());
    wlr_scene_rect_set_color(overview_.dim, dim);
  }

  overview_.previews.clear();

  const double scale = 1.0 + (WindowTuning::overviewZoom - 1.0) * overview_.progress;
  const double dragX = overview_.dragging ? d->cursor->x - overview_.pressX : 0.0;
  const double dragY = overview_.dragging ? d->cursor->y - overview_.pressY : 0.0;
  const auto *dropTarget =
      overview_.dragging
          ? overviewPreviewAt(d->cursor->x, d->cursor->y, overview_.pressed)
          : nullptr;
  const int dropWindow = dropTarget ? dropTarget->window : 0;
  const Rect screen(0, 0, output.width(), output.height());
  std::unordered_set<int> visibleWindows;

  // A fullscreen window that stepped aside is hidden at rest: its tile fades
  // out as the overview closes, and the others go back to their own places.
  const ClientWindow *aside = workspaceFullscreen();
  if (aside && !steppedAsideFor(*aside))
    aside = nullptr;
  const double progress = std::clamp(overview_.progress, 0.0, 1.0);
  const bool stacking = windowTemplate_ && windowTemplate_->allowOverlap;
  const auto previewable = [](const ClientWindow &client) {
    // A window a maximized one hides is not shown either: drawn underneath,
    // it could never be picked.
    return client.state.mapped && !client.state.minimized && !client.utility &&
           !client.layout.hiddenByMaximize && client.scene.tree &&
           client.layout.geometry.isValid();
  };

  const auto draw = [&](ClientWindow *client, const Rect &layout,
                        bool dragged) {
    Rect rect = overviewRect(layout, client->state.workspace);
    if (dragged)
      rect.translate(static_cast<int>(std::lround(dragX)),
                     static_cast<int>(std::lround(dragY)));
    if (!rect.intersects(screen))
      return;
    auto previewTree = overview_.previewTrees.find(client->id);
    wlr_scene_tree *previewTreeNode = nullptr;
    if (previewTree != overview_.previewTrees.end()) {
      previewTreeNode = previewTree->second;
    } else {
      previewTreeNode = wlr_scene_tree_create(overview_.tree);
      if (!previewTreeNode)
        return;
      overview_.previewTrees.emplace(client->id, previewTreeNode);
    }
    double pScale = scale;

    Rect shown = rect;
    // Fullscreen and spread-out stacking windows are drawn smaller than they
    // are, scaled into their tile.
    if ((client->state.fullscreen || stacking) &&
        client->layout.geometry.isValid() && layout.isValid()) {
      const auto &source = client->layout.geometry;
      const double fit = std::min(
          static_cast<double>(layout.width()) / source.width(),
          static_cast<double>(layout.height()) / source.height());
      pScale *= fit;
      const int contentWidth = static_cast<int>(
          std::lround(source.width() * pScale));
      const int contentHeight = static_cast<int>(
          std::lround(source.height() * pScale));
      shown = Rect(rect.x() + (rect.width() - contentWidth) / 2,
                   rect.y() + (rect.height() - contentHeight) / 2, contentWidth,
                   contentHeight);
    }
    wlr_scene_node_set_position(&previewTreeNode->node, shown.x(), shown.y());
    if (dragged)
      wlr_scene_node_raise_to_top(&previewTreeNode->node);
    static const float hover[4] = {1.0f, 1.0f, 1.0f, 0.35f};
    const bool ringed = client->id == overview_.hovered || client->id == dropWindow;
    auto &frame = overview_.previewFrames[client->id];
    updateFrame(overview_.tree, frame, shown, client == focused_ ? 3 : ringed ? 2 : 0,
                client == focused_ ? overview_.accent : hover);
    if (frame)
      wlr_scene_node_place_below(&frame->node, &previewTreeNode->node);
    visibleWindows.insert(client->id);
    auto &copies = overview_.previewBuffers[client->id];
    std::unordered_set<wlr_scene_buffer *> seen;
    // The window's alpha, not its buffers': one that just appeared (a new
    // subsurface or corner view) stays opaque until the glass pass sets it,
    // after this, and would flash the preview opaque for a frame.
    const bool fading = windowAnimations_ &&
                        windowAnimations_->animation().ownsOpacity(client->scene.tree);
    float opacity = fading ? -1.0f
                    : client->state.fullscreen || client->utility
                        ? 1.0f
                        : d->steadyWindowOpacity;
    if (client == aside && opacity >= 0.0f)
      opacity *= static_cast<float>(progress);
    scene_node_dfs(&client->scene.tree->node, previewTreeNode, 0.0, 0.0,
                 pScale, true, opacity, copies, seen);
    for (auto it = copies.begin(); it != copies.end();) {
      if (!seen.contains(it->first)) {
        wlr_scene_node_destroy(&it->second->node);
        it = copies.erase(it);
      } else {
        ++it;
      }
    }
    overview_.previews.push_back({client->id, client->state.workspace, shown});
  };

  // Other workspaces are laid out afresh: what the layout kept from their last
  // pass is stale (a window moved in keeps the place it had), and stale
  // places overlap, so one preview covers another and cannot be picked.
  std::unordered_map<int, std::vector<WindowPlacement>> laidOut;
  const Rect area = workArea();
  const auto placementOf =
      [&](const ClientWindow &client) -> const WindowPlacement * {
    auto found = laidOut.find(client.state.workspace);
    if (found == laidOut.end())
      found = laidOut
                  .emplace(
                      client.state.workspace,
                      windowLayout_->presentation(client.state.workspace, area))
                  .first;
    for (const auto &placement : found->second)
      if (static_cast<int>(placement.window) == client.id)
        return &placement;
    return nullptr;
  };


  struct Spread {
    int workspace;
    Rect column;
    Rect tile;
    // How far the neighbours move aside for the tile.
    double weight;
  };
  std::vector<Spread> spreads;
  // Where a fullscreen window would be if it were not fullscreen.
  const auto columnOf = [&](const ClientWindow &client) {
    if (client.state.floating)
      return floatingGeometry(client, area);
    if (const auto *placement = placementOf(client);
        placement && placement->geometry.isValid())
      return placement->geometry;
    return client.layout.geometry;
  };

  const auto fullscreenRect = [&](const ClientWindow &client) {
    const int centred = columnOf(client).center().x() - output.width() / 2;
    return Rect(static_cast<int>(std::lround(centred * (&client == aside ? 1.0
                                                                       : progress))),
                0, output.width(), output.height());
  };
  for (const auto &client : clients_)
    if (!stacking && client->state.fullscreen && client->state.mapped &&
        !client->state.minimized && !client->utility)
      spreads.push_back({client->state.workspace, columnOf(*client),
                         fullscreenRect(*client),
                         client.get() == aside ? progress : 1.0});


  const auto placedRect = [&](const ClientWindow &client) {
    if (client.state.workspace == workspace_ || client.state.floating) {
      // Where the window is drawn right now, so a focus move that scrolls
      // the columns animates in the preview too.
      if (!windowAnimations_ || !client.scene.tree)
        return client.layout.geometry;
      const auto frame = windowAnimations_->animation().visualGeometry(
          client.scene.tree, animationRectangle(client.layout.geometry));
      return Rect(frame.left, frame.top, frame.width, frame.height);
    }
    if (const auto *placement = placementOf(client);
        placement && placement->geometry.isValid())
      return placement->geometry;
    return client.layout.geometry;
  };
  // Stacking: each workspace's windows spread over it, moving out from where
  // they are as the overview opens.
  std::unordered_map<int, Rect> spreadPlaces;
  if (stacking) {
    std::unordered_map<int, std::vector<const ClientWindow *>> byWorkspace;
    for (const auto &client : clients_)
      if (previewable(*client))
        byWorkspace[client->state.workspace].push_back(client.get());
    for (const auto &[workspace, windows] : byWorkspace) {
      std::vector<Rect> sizes;
      for (const auto *client : windows)
        sizes.push_back(client->layout.geometry);
      const auto places = spreadOut(sizes, area);
      for (std::size_t index = 0; index < windows.size(); ++index)
        spreadPlaces[windows[index]->id] = places[index];
    }
  }
  const auto layoutRect = [&](const ClientWindow &client) {
    if (const auto place = spreadPlaces.find(client.id); place != spreadPlaces.end())
      return interpolated(placedRect(client), place->second, progress);
    if (client.state.fullscreen)
      return fullscreenRect(client);

    const bool spreadOut =
        std::ranges::any_of(spreads, [&](const Spread &spread) {
          return spread.workspace == client.state.workspace;
        });
    const auto *placement = spreadOut ? placementOf(client) : nullptr;
    Rect rect = placement && placement->geometry.isValid() ? placement->geometry
                                                           : placedRect(client);
    for (const auto &spread : spreads) {
      if (spread.workspace != client.state.workspace)
        continue;
      // Neighbours keep their distance to the column's edge, now the tile's.
      const bool left = rect.center().x() < spread.column.center().x();
      const int shift = left ? spread.tile.x() - spread.column.x()
                             : spread.tile.x() + spread.tile.width() -
                                   (spread.column.x() + spread.column.width());
      rect.translate(static_cast<int>(std::lround(shift * spread.weight)), 0);
    }
    return rect;
  };

  const bool openSettled = overview_.open && overview_.progress >= 1.0;
  std::unordered_set<int> springing;
  const auto tileRect = [&](const ClientWindow &client) {
    const Rect target = layoutRect(client);
    const bool spread =
        std::ranges::any_of(spreads, [&](const Spread &spread) {
          return spread.workspace == client.state.workspace;
        });
    if ((!spread && !stacking) || !openSettled)
      return target;
    springing.insert(client.id);
    auto [found, fresh] = overview_.tileMotion.try_emplace(
        client.id, OverviewState::TileMotion{static_cast<double>(target.x()),
                                             static_cast<double>(target.y())});
    auto &motion = found->second;
    if (!fresh) {
      double dx = motion.x - target.x();
      double dy = motion.y - target.y();
      stepSceneSpring(dx, motion.vx, WindowTuning::overviewViewSpring,
                      overview_.stepSeconds);
      stepSceneSpring(dy, motion.vy, WindowTuning::overviewViewSpring,
                      overview_.stepSeconds);
      motion.x = target.x() + dx;
      motion.y = target.y() + dy;
    }
    return Rect(static_cast<int>(std::lround(motion.x)),
                static_cast<int>(std::lround(motion.y)), target.width(),
                target.height());
  };

  // Drawn in their stacking order, so windows still overlapping as the
  // overview opens or closes keep the one on top.
  std::vector<ClientWindow *> drawOrder;
  if (d->normalLayer) {
    wlr_scene_node *node = nullptr;
    wl_list_for_each(node, &d->normalLayer->children, link)
      for (const auto &client : clients_)
        if (client->scene.tree && &client->scene.tree->node == node)
          drawOrder.push_back(client.get());
  }
  for (const auto &client : clients_)
    if (std::ranges::find(drawOrder, client.get()) == drawOrder.end())
      drawOrder.push_back(client.get());
  ClientWindow *dragged = nullptr;
  for (auto *client : drawOrder) {
    if (!previewable(*client))
      continue;
    if (overview_.dragging && client->id == overview_.pressed) {
      dragged = client;
      continue;
    }
    draw(client, tileRect(*client), false);
  }
  std::erase_if(overview_.tileMotion,
                [&](const auto &entry) { return !springing.contains(entry.first); });
  // The dragged window follows the pointer above the others.
  if (dragged)
    draw(dragged, layoutRect(*dragged), true);

  for (auto it = overview_.previewTrees.begin();
       it != overview_.previewTrees.end();) {
    if (!visibleWindows.contains(it->first)) {
      wlr_scene_node_destroy(&it->second->node);
      overview_.previewBuffers.erase(it->first);
      if (const auto frame = overview_.previewFrames.find(it->first);
          frame != overview_.previewFrames.end()) {
        if (frame->second)
          wlr_scene_node_destroy(&frame->second->node);
        overview_.previewFrames.erase(frame);
      }
      it = overview_.previewTrees.erase(it);
    } else {
      ++it;
    }
  }
}

void WaylandCompositor::advanceOverview(std::uint64_t presentNs) {
  if (!d || !d->overviewLayer || !overviewShown())
    return;
  const double seconds =
      presentNs > overview_.lastStep
          ? std::min(0.1, static_cast<double>(presentNs - overview_.lastStep) / 1e9)
          : 0.0;
  overview_.lastStep = std::max(overview_.lastStep, presentNs);
  const double target = overview_.open ? 1.0 : 0.0;
  if (!windowAnimations_ || windowAnimations_->animation().duration() <= 0) {
    overview_.progress = target;
    overview_.velocity = 0.0;
    overview_.view = workspace_;
    overview_.viewVelocity = 0.0;
  } else {
    double offset = overview_.progress - target;
    stepSceneSpring(offset, overview_.velocity, WindowTuning::overviewSpring, seconds);
    overview_.progress = target + offset;
    // The view moves in workspaces; the spring settles in pixels.
    const double band = d->outputSize().height() + WindowTuning::overviewWorkspaceGap;
    double viewOffset = (overview_.view - workspace_) * band;
    double viewSpeed = overview_.viewVelocity * band;
    stepSceneSpring(viewOffset, viewSpeed, WindowTuning::overviewViewSpring, seconds);
    overview_.view = workspace_ + viewOffset / band;
    overview_.viewVelocity = viewSpeed / band;
    if (std::abs(viewOffset) < WindowTuning::overviewViewSpring.restDistance &&
        std::abs(viewSpeed) < WindowTuning::overviewViewSpring.restSpeed) {
      overview_.view = workspace_;
      overview_.viewVelocity = 0.0;
    }
  }
  const auto &spring = WindowTuning::overviewSpring;
  const bool settled = std::abs(overview_.progress - target) < spring.restDistance &&
                       std::abs(overview_.velocity) < spring.restSpeed;

  if (!overview_.open && (settled || overview_.progress <= 0.0)) {
    overview_.progress = 0.0;
    overview_.velocity = 0.0;
    if (overview_.tree) {
      wlr_scene_node_destroy(&overview_.tree->node);
      overview_.tree = nullptr;
      overview_.dim = nullptr;
    }
    overview_.previews.clear();
    overview_.previewTrees.clear();
    overview_.previewFrames.clear();
    overview_.previewBuffers.clear();
    overview_.tileMotion.clear();
    for (auto *layer : {d->normalLayer, d->fullscreenLayer, d->animationLayer})
      if (layer)
        wlr_scene_node_set_enabled(&layer->node, true);
    d->schedulePointerRebase();
    return;
  }
  if (overview_.open && settled)
    overview_.progress = 1.0;
  overview_.stepSeconds = seconds;
  for (auto *layer : {d->normalLayer, d->fullscreenLayer, d->animationLayer})
    if (layer)
      wlr_scene_node_set_enabled(&layer->node, false);
  rebuildOverviewPreviews();
}

bool WaylandCompositor::pointerOverShell() const {
  double sx = 0.0;
  double sy = 0.0;
  auto *surface = d->surfaceAt(d->cursor->x, d->cursor->y, &sx, &sy);
  auto *root = surface ? wlr_surface_get_root_surface(surface) : nullptr;
  for (const auto *layer : d->layers)
    if (root && layer->surface && layer->surface->surface == root &&
        layer->surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP)
      return true;
  return false;
}

bool WaylandCompositor::overviewPointerMotion() {
  if (!overview_.open || overview_.shellPress || pointerOverShell())
    return false;
  wlr_seat_pointer_notify_clear_focus(d->seat);
  if (overview_.pressed && !overview_.dragging &&
      std::hypot(d->cursor->x - overview_.pressX, d->cursor->y - overview_.pressY) >
          WindowTuning::overviewDragThreshold)
    overview_.dragging = true;
  const auto *hovered = overviewPreviewAt(d->cursor->x, d->cursor->y);
  overview_.hovered = hovered ? hovered->window : 0;
  d->setCursorImage(overview_.dragging ? "grabbing" : "default");
  return true;
}

void WaylandCompositor::moveClientToWorkspace(ClientWindow *client, int workspace) {
  if (!client || client->utility)
    return;
  client->state.workspace =
      std::clamp(workspace, 0, std::max(0, workspaceCount() - 1));
  windowLayout_->moveToWorkspace(client->id, client->state.workspace);
  arrange();
  synchronizeWindowFocus();
}

bool WaylandCompositor::overviewPointerButton(std::uint32_t button, bool pressed) {
  if (!overview_.open) {
    overview_.shellPress = false;
    return false;
  }
  // The shell (bar, launcher) above the overview gets its press and the
  // matching release, wherever the pointer is released.
  if (pressed && pointerOverShell()) {
    overview_.shellPress = true;
    return false;
  }
  if (!pressed && overview_.shellPress) {
    overview_.shellPress = false;
    return false;
  }
  const double x = d->cursor->x;
  const double y = d->cursor->y;
  if (button == BTN_MIDDLE) {
    if (!pressed)
      if (const auto *preview = overviewPreviewAt(x, y))
        closeClient(clientById(preview->window));
    return true;
  }
  if (button != BTN_LEFT)
    return true;
  if (pressed) {
    const auto *preview = overviewPreviewAt(x, y);
    overview_.pressed = preview ? preview->window : 0;
    overview_.pressX = x;
    overview_.pressY = y;
    overview_.dragging = false;
    return true;
  }

  auto *client = clientById(overview_.pressed);
  if (overview_.dragging && client) {
    // Dropped on a window: swap with it on its workspace, or move there first.
    if (const auto *target = overviewPreviewAt(x, y, client->id)) {
      if (target->workspace != client->state.workspace)
        moveClientToWorkspace(client, target->workspace);
      // Stacking windows keep their own places: only the workspace changes.
      if (!windowTemplate_->allowOverlap &&
          performLayoutAction("swap", {{"window", client->id}, {"target", target->window}})) {
        arrange();
        synchronizeWindowFocus();
      }
    } else {
      const int workspace = overviewWorkspaceAt(y);
      if (workspace != client->state.workspace)
        moveClientToWorkspace(client, workspace);
    }
  } else if (client) {
    // A click switches to the window.
    selectWorkspace(client->state.workspace);
    client->state.minimized = false;
    windowLayout_->setMinimized(client->id, false);
    windowLayout_->focus(client->id);
    arrange();
    focus(client);
    closeOverview();
  } else {
    // A click beside the windows switches to that workspace.
    selectWorkspace(overviewWorkspaceAt(y));
    closeOverview();
  }
  overview_.pressed = 0;
  overview_.dragging = false;
  return true;
}

bool WaylandCompositor::overviewAxis(double delta, bool horizontal) {
  if (!overview_.open || pointerOverShell())
    return false;
  auto &scroll = horizontal ? overview_.scrollHorizontal : overview_.scroll;
  (horizontal ? overview_.scroll : overview_.scrollHorizontal) = 0.0;
  scroll += delta;
  if (std::abs(scroll) < WindowTuning::overviewScrollStep)
    return true;
  const int step = scroll > 0 ? 1 : -1;
  scroll = 0.0;
  if (horizontal)
    runAction(step > 0 ? "focus-column-right" : "focus-column-left");
  else
    selectWorkspace(std::clamp(workspace_ + step, 0, std::max(0, workspaceCount() - 1)));
  return true;
}

} // namespace LunaDash
