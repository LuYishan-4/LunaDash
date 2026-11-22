#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/animation/AnimationProfile.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTuning.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/json/JsonAccess.hpp"

#include <algorithm>
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

// "#rrggbb" into RGBA; false when it is not one.
bool parseHexColor(const std::string &text, float rgba[4]) {
  if (text.size() != 7 || text.front() != '#')
    return false;
  for (int channel = 0; channel < 3; ++channel) {
    const auto value = std::strtol(text.substr(1 + channel * 2, 2).c_str(), nullptr, 16);
    rgba[channel] = static_cast<float>(value) / 255.0f;
  }
  rgba[3] = 1.0f;
  return true;
}

// Copies the buffers under `node` into `parent`, scaled. The window's own tree
// may be disabled (other workspaces, or the hidden real windows); the nodes
// inside it keep their enabled state.
void clonePreview(wlr_scene_node *node, wlr_scene_tree *parent, double x, double y,
                  double scale, bool root,
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
        clonePreview(child, parent, x, y, scale, false, copies, seen);
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
  wlr_scene_buffer_set_opacity(copy, buffer->opacity);
  wlr_scene_node_set_position(&copy->node, static_cast<int>(std::lround(x * scale)),
                              static_cast<int>(std::lround(y * scale)));
}

// The ring around a preview: `width` wide in `color`, or hidden for a width of 0.
// It lives as long as the preview, so it is updated, never created again.
void updateFrame(wlr_scene_tree *parent, wlr_scene_rect *&frame, const Rect &rect,
                 int width, const float color[4]) {
  if (!frame && width > 0)
    frame = wlr_scene_rect_create(parent, 1, 1, color);
  if (!frame)
    return;
  wlr_scene_node_set_enabled(&frame->node, width > 0);
  if (width <= 0)
    return;
  wlr_scene_rect_set_size(frame, rect.width() + 2 * width, rect.height() + 2 * width);
  wlr_scene_rect_set_color(frame, color);
  wlr_scene_node_set_position(&frame->node, rect.x() - width, rect.y() - width);
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
  if (d)
    for (const auto *output : d->outputs)
      wlr_output_schedule_frame(output->output);
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
  if (overview_.tree) {
    // Preserve preview nodes during a frame; update their geometry and buffers
    // instead of destroying and recreating every scene texture.
  } else if (!overviewShown() || !d || !d->overviewLayer) {
    overview_.previews.clear();
    return;
  } else {
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
    wlr_scene_node_set_position(&previewTreeNode->node, rect.x(), rect.y());
    // The dragged window is drawn above the others.
    if (dragged)
      wlr_scene_node_raise_to_top(&previewTreeNode->node);
    static const float hover[4] = {1.0f, 1.0f, 1.0f, 0.35f};
    const bool ringed = client->id == overview_.hovered || client->id == dropWindow;
    auto &frame = overview_.previewFrames[client->id];
    updateFrame(overview_.tree, frame, rect, client == focused_ ? 3 : ringed ? 2 : 0,
                client == focused_ ? overview_.accent : hover);
    if (frame)
      wlr_scene_node_place_below(&frame->node, &previewTreeNode->node);
    visibleWindows.insert(client->id);
    auto &copies = overview_.previewBuffers[client->id];
    std::unordered_set<wlr_scene_buffer *> seen;
    clonePreview(&client->scene.tree->node, previewTreeNode, 0.0, 0.0, scale,
                 true, copies, seen);
    for (auto it = copies.begin(); it != copies.end();) {
      if (!seen.contains(it->first)) {
        wlr_scene_node_destroy(&it->second->node);
        it = copies.erase(it);
      } else {
        ++it;
      }
    }
    overview_.previews.push_back({client->id, client->state.workspace, rect});
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

  // Where each window is laid out: the current workspace as shown, the others
  // as their layout places them now.
  const auto layoutRect = [&](const ClientWindow &client) {
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

  ClientWindow *dragged = nullptr;
  for (const auto &client : clients_) {
    // A window a maximized one hides is not shown either: drawn underneath,
    // it could never be picked.
    if (!client->state.mapped || client->state.minimized || client->utility ||
        client->layout.hiddenByMaximize || !client->scene.tree ||
        !client->layout.geometry.isValid())
      continue;
    if (overview_.dragging && client->id == overview_.pressed) {
      dragged = client.get();
      continue;
    }
    draw(client.get(), layoutRect(*client), false);
  }
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
    for (auto *layer : {d->normalLayer, d->fullscreenLayer, d->animationLayer})
      if (layer)
        wlr_scene_node_set_enabled(&layer->node, true);
    d->schedulePointerRebase();
    return;
  }
  if (overview_.open && settled)
    overview_.progress = 1.0;
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
      if (performLayoutAction("swap", {{"window", client->id}, {"target", target->window}})) {
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
