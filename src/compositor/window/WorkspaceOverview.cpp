// The workspace overview, like niri's: the screen zooms out, every workspace is
// stacked vertically around the current one, and each window is drawn live where
// the layout places it. The real windows are hidden meanwhile; their previews are
// copies of their current scene buffers, rebuilt every frame.

#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTuning.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/json/JsonAccess.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <linux/input-event-codes.h>
#include <string>

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
                  double scale, bool root) {
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
        clonePreview(child, parent, x, y, scale, false);
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
  auto *copy = wlr_scene_buffer_create(parent, buffer->buffer);
  if (!copy)
    return;
  copy->point_accepts_input = [](wlr_scene_buffer *, double *, double *) {
    return false;
  };
  wlr_scene_buffer_set_source_box(copy, &buffer->src_box);
  wlr_scene_buffer_set_transform(copy, buffer->transform);
  wlr_scene_buffer_set_dest_size(
      copy, std::max(1, static_cast<int>(std::lround(width * scale))),
      std::max(1, static_cast<int>(std::lround(height * scale))));
  wlr_scene_buffer_set_opacity(copy, buffer->opacity);
  wlr_scene_node_set_position(&copy->node, static_cast<int>(std::lround(x * scale)),
                              static_cast<int>(std::lround(y * scale)));
}

void addFrame(wlr_scene_tree *parent, const Rect &rect, int width, const float color[4]) {
  if (auto *frame = wlr_scene_rect_create(parent, rect.width() + 2 * width,
                                          rect.height() + 2 * width, color))
    wlr_scene_node_set_position(&frame->node, rect.x() - width, rect.y() - width);
}

} // namespace

bool WaylandCompositor::overviewShown() const {
  return overview_.open || overview_.progress > 0.0 || overview_.tree;
}

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
  for (const auto *output : d->outputs)
    wlr_output_schedule_frame(output->output);
}

void WaylandCompositor::closeOverview() {
  overview_.open = false;
  overview_.pressed = 0;
  overview_.dragging = false;
  overview_.hovered = 0;
  overview_.scroll = 0.0;
  if (d)
    for (const auto *output : d->outputs)
      wlr_output_schedule_frame(output->output);
}

Rect WaylandCompositor::overviewRect(const Rect &rectangle, int workspace) const {
  const Size output = d->outputSize();
  const double scale = 1.0 + (WindowTuning::overviewZoom - 1.0) * overview_.progress;
  const double centerX = output.width() / 2.0;
  const double centerY = output.height() / 2.0;
  const double offset = static_cast<double>(workspace - workspace_) *
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
  const int workspace =
      workspace_ + static_cast<int>(std::floor(unscaled / band));
  return std::clamp(workspace, 0, std::max(0, workspaceCount() - 1));
}

void WaylandCompositor::rebuildOverviewPreviews() {
  if (overview_.tree) {
    wlr_scene_node_destroy(&overview_.tree->node);
    overview_.tree = nullptr;
  }
  overview_.previews.clear();
  if (!overviewShown() || !d->overviewLayer)
    return;
  overview_.tree = wlr_scene_tree_create(d->overviewLayer);
  if (!overview_.tree)
    return;
  const Size output = d->outputSize();
  // Dim the wallpaper so the windows stand out.
  const float dim[4] = {0.0f, 0.0f, 0.0f,
                        static_cast<float>(0.35 * std::clamp(overview_.progress, 0.0, 1.0))};
  wlr_scene_rect_create(overview_.tree, output.width(), output.height(), dim);

  const double scale = 1.0 + (WindowTuning::overviewZoom - 1.0) * overview_.progress;
  const double dragX = overview_.dragging ? d->cursor->x - overview_.pressX : 0.0;
  const double dragY = overview_.dragging ? d->cursor->y - overview_.pressY : 0.0;
  const auto *dropTarget =
      overview_.dragging
          ? overviewPreviewAt(d->cursor->x, d->cursor->y, overview_.pressed)
          : nullptr;
  const int dropWindow = dropTarget ? dropTarget->window : 0;
  const Rect screen(0, 0, output.width(), output.height());

  const auto draw = [&](ClientWindow *client, const Rect &layout, bool dragged) {
    Rect rect = overviewRect(layout, client->workspace);
    if (dragged)
      rect.translate(static_cast<int>(std::lround(dragX)),
                     static_cast<int>(std::lround(dragY)));
    if (!rect.intersects(screen))
      return;
    if (client == focused_)
      addFrame(overview_.tree, rect, 3, overview_.accent);
    else if (client->id == overview_.hovered || client->id == dropWindow) {
      const float hover[4] = {1.0f, 1.0f, 1.0f, 0.35f};
      addFrame(overview_.tree, rect, 2, hover);
    }
    auto *preview = wlr_scene_tree_create(overview_.tree);
    if (!preview)
      return;
    wlr_scene_node_set_position(&preview->node, rect.x(), rect.y());
    clonePreview(&client->sceneTree->node, preview, 0.0, 0.0, scale, true);
    overview_.previews.push_back({client->id, client->workspace, rect});
  };

  // Where each window is laid out: the current workspace as shown, the others
  // as they were last laid out.
  const auto layoutRect = [this](const ClientWindow &client) {
    if (client.workspace == workspace_ || client.floating)
      return client.geometry;
    for (const auto &column : windowLayout_->snapshot(client.workspace).columns)
      if (static_cast<int>(column.window) == client.id && column.geometry.isValid())
        return column.geometry;
    return client.geometry;
  };

  ClientWindow *dragged = nullptr;
  for (const auto &client : clients_) {
    if (!client->mapped || client->minimized || client->utility ||
        !client->sceneTree || !client->geometry.isValid())
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
  } else {
    double offset = overview_.progress - target;
    stepSceneSpring(offset, overview_.velocity, WindowTuning::overviewSpring, seconds);
    overview_.progress = target + offset;
  }
  const auto &spring = WindowTuning::overviewSpring;
  const bool settled = std::abs(overview_.progress - target) < spring.restDistance &&
                       std::abs(overview_.velocity) < spring.restSpeed;

  if (!overview_.open && (settled || overview_.progress <= 0.0)) {
    overview_.progress = 0.0;
    overview_.velocity = 0.0;
    rebuildOverviewPreviews();
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
  client->workspace = std::clamp(workspace, 0, std::max(0, workspaceCount() - 1));
  windowLayout_->moveToWorkspace(client->id, client->workspace);
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
      if (target->workspace != client->workspace)
        moveClientToWorkspace(client, target->workspace);
      if (performLayoutAction("swap", {{"window", client->id}, {"target", target->window}})) {
        arrange();
        synchronizeWindowFocus();
      }
    } else {
      const int workspace = overviewWorkspaceAt(y);
      if (workspace != client->workspace)
        moveClientToWorkspace(client, workspace);
    }
  } else if (client) {
    // A click switches to the window.
    selectWorkspace(client->workspace);
    client->minimized = false;
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

bool WaylandCompositor::overviewAxis(double delta) {
  if (!overview_.open || pointerOverShell())
    return false;
  overview_.scroll += delta;
  if (std::abs(overview_.scroll) >= WindowTuning::overviewScrollStep) {
    const int step = overview_.scroll > 0 ? 1 : -1;
    overview_.scroll = 0.0;
    selectWorkspace(std::clamp(workspace_ + step, 0, std::max(0, workspaceCount() - 1)));
  }
  return true;
}

bool WaylandCompositor::overviewKey(std::uint32_t symbol, std::uint32_t modifiers) {
  if (!overview_.open)
    return false;
  const bool control = modifiers & WLR_MODIFIER_CTRL;
  switch (symbol) {
  case XKB_KEY_Escape:
  case XKB_KEY_Return:
  case XKB_KEY_KP_Enter:
    closeOverview();
    return true;
  case XKB_KEY_Left:
  case XKB_KEY_h:
    handleShortcut(control ? "reorderLeft" : "focusLeft");
    return true;
  case XKB_KEY_Right:
  case XKB_KEY_l:
    handleShortcut(control ? "reorderRight" : "focusRight");
    return true;
  case XKB_KEY_Up:
  case XKB_KEY_k:
  case XKB_KEY_Down:
  case XKB_KEY_j: {
    const int step = symbol == XKB_KEY_Up || symbol == XKB_KEY_k ? -1 : 1;
    const int workspace =
        std::clamp(workspace_ + step, 0, std::max(0, workspaceCount() - 1));
    if (control && focused_)
      moveClientToWorkspace(focused_, workspace);
    selectWorkspace(workspace);
    return true;
  }
  default:
    return false;
  }
}

} // namespace LunaDash
