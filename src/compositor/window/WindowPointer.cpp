#include "compositor/client/ClientSurface.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/layout/FreeformLayout.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/WindowTuning.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <linux/input-event-codes.h>
#include <vector>

namespace LunaDash {
namespace {

// xdg-shell edges.
constexpr std::uint32_t edgeTop = 1, edgeBottom = 2, edgeLeft = 4, edgeRight = 8;

// Stacking: windows overlap and the pointer moves and resizes them freely.
bool freeform(const WindowTemplate *windowTemplate, const ClientWindow &client) {
  return windowTemplate && windowTemplate->allowOverlap &&
         windowTemplate->clientMoveResize && !client.state.floating;
}

// The edges of `geometry` nearest to `point`, by thirds; from the middle, the
// bottom-right corner.
std::uint32_t nearestEdges(const Rect &geometry, Point point) {
  std::uint32_t edges = 0;
  if (point.x() < geometry.x() + geometry.width() / 3)
    edges |= edgeLeft;
  else if (point.x() > geometry.right() - geometry.width() / 3)
    edges |= edgeRight;
  if (point.y() < geometry.y() + geometry.height() / 3)
    edges |= edgeTop;
  else if (point.y() > geometry.bottom() - geometry.height() / 3)
    edges |= edgeBottom;
  return edges ? edges : edgeBottom | edgeRight;
}

// `start` with its dragged edges moved by `moved` and the others where they
// were: never smaller than `minimum`, nor dragged past `area`.
Rect resizedGeometry(const Rect &start, std::uint32_t edges, Point moved,
                     Size minimum, const Rect &area) {
  int left = start.x(), top = start.y();
  int right = start.x() + start.width(), bottom = start.y() + start.height();
  const int areaRight = area.x() + area.width();
  const int areaBottom = area.y() + area.height();
  if (edges & edgeLeft)
    left = std::clamp(left + moved.x(), std::min(area.x(), right - minimum.width()),
                      right - minimum.width());
  if (edges & edgeRight)
    right = std::clamp(right + moved.x(), left + minimum.width(),
                       std::max(areaRight, left + minimum.width()));
  if (edges & edgeTop)
    top = std::clamp(top + moved.y(), std::min(area.y(), bottom - minimum.height()),
                     bottom - minimum.height());
  if (edges & edgeBottom)
    bottom = std::clamp(bottom + moved.y(), top + minimum.height(),
                        std::max(areaBottom, top + minimum.height()));
  return Rect(left, top, right - left, bottom - top);
}

// `geometry` shifted so that an edge within `distance` of the area's edges or
// of a neighbour's lands on it, like magnetic window borders.
Rect snapped(Rect geometry, const Rect &area, const std::vector<Rect> &others,
             int distance) {
  int bestX = distance + 1, bestY = distance + 1, shiftX = 0, shiftY = 0;
  const auto consider = [](int edge, int target, int &best, int &shift) {
    if (std::abs(target - edge) < best) {
      best = std::abs(target - edge);
      shift = target - edge;
    }
  };
  const int left = geometry.x(), right = geometry.x() + geometry.width();
  const int top = geometry.y(), bottom = geometry.y() + geometry.height();
  consider(left, area.x(), bestX, shiftX);
  consider(right, area.x() + area.width(), bestX, shiftX);
  consider(top, area.y(), bestY, shiftY);
  consider(bottom, area.y() + area.height(), bestY, shiftY);
  for (const auto &other : others) {
    const int otherRight = other.x() + other.width();
    const int otherBottom = other.y() + other.height();
    if (top < otherBottom && bottom > other.y())
      for (const int target : {other.x(), otherRight}) {
        consider(left, target, bestX, shiftX);
        consider(right, target, bestX, shiftX);
      }
    if (left < otherRight && right > other.x())
      for (const int target : {other.y(), otherBottom}) {
        consider(top, target, bestY, shiftY);
        consider(bottom, target, bestY, shiftY);
      }
  }
  geometry.translate(shiftX, shiftY);
  return geometry;
}

// Where a window dragged with the pointer at `cursor` quick-tiles: the left or
// right half (a quarter near a corner), or the top edge alone, which maximizes.
std::uint32_t quickTileEdges(PointF cursor, Size output) {
  const int edge = WindowTuning::quickTileEdge;
  std::uint32_t edges = cursor.x() < edge                   ? edgeLeft
                        : cursor.x() >= output.width() - edge ? edgeRight
                                                              : 0;
  if (!edges)
    return cursor.y() < edge ? edgeTop : 0;
  if (cursor.y() < output.height() / 4.0)
    edges |= edgeTop;
  else if (cursor.y() >= output.height() * 3 / 4.0)
    edges |= edgeBottom;
  return edges;
}

// The layout's slot for `client` on the current workspace.
const WindowPlacement *slotOf(const WorkspaceLayoutSnapshot &snapshot,
                              const ClientWindow &client) {
  for (const auto &slot : snapshot.columns)
    if (slot.window == static_cast<LayoutWindowId>(client.id))
      return &slot;
  return nullptr;
}

} // namespace

void WaylandCompositor::Impl::startWindowPointer(ClientWindow *client,
                                                 uint32_t button, uint32_t edges,
                                                 bool clientGrab) {
  q->focus(client);
  pointerWindow = client->id;
  pointerButton = button;
  pointerClientGrab = clientGrab;
  pointerResizeEdges = edges;
  pointerResize = edges != 0;
  pointerQuickTile = 0;
  const bool stacking = freeform(q->windowTemplate_, *client);
  const auto snapshot = q->windowLayout_->snapshot(client->state.workspace);
  const auto *slot = slotOf(snapshot, *client);
  const bool tiled = stacking && slot && jsonInt(slot->metadata, "tile") != 0;
  // A maximized or tiled stacking window being moved keeps its place until the
  // pointer moves, then goes back to its own size under it.
  pointerRestore = stacking && !pointerResize && (client->state.maximized || tiled);
  if (client->state.maximized && !pointerRestore) {
    q->setMaximized(client, false);
    q->arrange();
  }
  // Resizing a tiled window starts from the tile and leaves it.
  if (tiled && pointerResize) {
    const auto &geometry = client->layout.geometry;
    performWindowLayoutAction(*q->windowLayout_, *q->windowTemplate_, "place",
                              {{"window", client->id}, {"x", geometry.x()},
                               {"y", geometry.y()}, {"width", geometry.width()},
                               {"height", geometry.height()}});
  }
  pointerLast = PointF(cursor->x, cursor->y);
  pointerOrigin = pointerLast;
  pointerStartGeometry = client->layout.geometry;
  pointerPublishTimeValid = false;
  pointerTarget = pointerEdge = 0;
  client->layout.manualResize = true;
  if (clientToplevel(*client))
    wlr_xdg_toplevel_set_resizing(clientToplevel(*client), pointerResize);
  setCursorImage(pointerResize ? windowResizeCursor(edges) : "grabbing");
}

// Alt and the left button moves the window under the pointer; Alt and the
// right button (or Alt+Shift and the left) resizes it from its nearest edges.
bool WaylandCompositor::Impl::beginWindowPointer(uint32_t button) {
  auto *keyboard = preferredKeyboard();
  if (!keyboard || (button != BTN_LEFT && button != BTN_RIGHT) ||
      !(wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_ALT))
    return false;
  double sx = 0, sy = 0;
  auto *client = clientForSurface(surfaceAt(cursor->x, cursor->y, &sx, &sy));
  if (!client || !q->windowTemplate_ ||
      !windowAllowsPointerInteraction(*q->windowTemplate_, *client))
    return false;
  const bool resize = button == BTN_RIGHT ||
                      (wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_SHIFT);
  // Tiled columns grow to the right and down; free windows from any edge.
  const std::uint32_t edges =
      !resize ? 0
      : windowAllowsClientMoveResize(*q->windowTemplate_, *client)
          ? nearestEdges(client->layout.geometry,
                         PointF(cursor->x, cursor->y).toPoint())
          : edgeBottom | edgeRight;
  startWindowPointer(client, button, edges, false);
  wlr_seat_pointer_notify_clear_focus(seat);
  return true;
}

bool WaylandCompositor::Impl::beginBorderResize(uint32_t button) {
  ClientWindow *client = nullptr;
  const auto edges =
      button == BTN_LEFT ? resizeEdgesAt(cursor->x, cursor->y, &client) : 0;
  if (!edges)
    return false;
  startWindowPointer(client, button, edges, false);
  return true;
}

uint32_t WaylandCompositor::Impl::resizeEdgesAt(double x, double y,
                                                ClientWindow **client) const {
  if (!q->windowTemplate_ || !normalLayer || q->overviewOpen() ||
      q->pointerOverShell())
    return 0;
  const Point point(static_cast<int>(std::floor(x)),
                    static_cast<int>(std::floor(y)));
  const int border = WindowTuning::resizeBorder;
  const int corner = WindowTuning::resizeCorner;
  // From the top window down: the first window under the pointer has it.
  wlr_scene_node *node = nullptr;
  wl_list_for_each_reverse(node, &normalLayer->children, link) {
    if (!node->enabled)
      continue;
    const auto found = std::find_if(
        q->clients_.begin(), q->clients_.end(), [node](const auto &candidate) {
          return candidate->scene.tree && &candidate->scene.tree->node == node;
        });
    if (found == q->clients_.end())
      continue;
    auto *candidate = found->get();
    const Rect geometry = candidate->layout.geometry;
    if (!candidate->state.mapped || candidate->state.minimized ||
        !geometry.isValid())
      continue;
    if (geometry.contains(point))
      return 0;
    if (candidate->state.maximized ||
        !windowAllowsClientMoveResize(*q->windowTemplate_, *candidate) ||
        !geometry.adjusted(-border, -border, border, border).contains(point))
      continue;
    // Past a side, or within `corner` of a corner along the edge beyond it.
    const bool beyondX = point.x() < geometry.x() || point.x() > geometry.right();
    const bool beyondY = point.y() < geometry.y() || point.y() > geometry.bottom();
    std::uint32_t edges = 0;
    if (point.x() < geometry.x() + (beyondY ? corner : 0))
      edges |= edgeLeft;
    else if (point.x() > geometry.right() - (beyondY ? corner : 0))
      edges |= edgeRight;
    if (point.y() < geometry.y() + (beyondX ? corner : 0))
      edges |= edgeTop;
    else if (point.y() > geometry.bottom() - (beyondX ? corner : 0))
      edges |= edgeBottom;
    *client = candidate;
    return edges;
  }
  return 0;
}

void WaylandCompositor::Impl::showQuickTile(uint32_t edges, const Rect &outline,
                                            ClientWindow *client) {
  pointerQuickTile = edges;
  if (!edges || !client || !client->scene.tree || !normalLayer) {
    if (quickTileOutline)
      wlr_scene_node_set_enabled(&quickTileOutline->node, false);
    return;
  }
  static const float color[4] = {0.3f, 0.3f, 0.3f, 0.3f};
  if (!quickTileOutline)
    quickTileOutline = wlr_scene_rect_create(normalLayer, 1, 1, color);
  if (!quickTileOutline)
    return;
  wlr_scene_rect_set_size(quickTileOutline, outline.width(), outline.height());
  wlr_scene_node_set_position(&quickTileOutline->node, outline.x(), outline.y());
  wlr_scene_node_set_enabled(&quickTileOutline->node, true);
  // Under the window being dragged, above the others.
  if (client->scene.tree->node.parent == normalLayer)
    wlr_scene_node_place_below(&quickTileOutline->node, &client->scene.tree->node);
}

bool WaylandCompositor::Impl::beginClientWindowPointer(ClientWindow *client,
                                                   uint32_t serial,
                                                   uint32_t edges,
                                                   bool validateSerial) {
  if (!client || !q->windowTemplate_ || pointerWindow ||
      !clientSurface(*client))
    return false;
  // A titlebar move works in both tiling and stacking: in tiling it chooses
  // another slot to swap with; a stacking window follows the pointer. Resizing
  // obeys the active template.
  if (edges == 0) {
    if (!windowAllowsPointerInteraction(*q->windowTemplate_, *client))
      return false;
  } else if (!windowAllowsClientMoveResize(*q->windowTemplate_, *client)) {
    return false;
  }
  if (validateSerial) {
    if (!wlr_seat_validate_pointer_grab_serial(seat, clientSurface(*client),
                                               serial))
      return false;
  } else if (!seat->pointer_state.grab_button) {
    return false;
  }
  if ((edges & ~15u) || (edges & 3u) == 3u || (edges & 12u) == 12u)
    return false;
  startWindowPointer(client, seat->pointer_state.grab_button, edges, true);
  return true;
}

bool WaylandCompositor::Impl::updateWindowPointer() {
  if (!pointerWindow)
    return false;
  const auto found = std::find_if(
      q->clients_.begin(), q->clients_.end(), [this](const auto &c) {
        return c->id == pointerWindow && c->state.mapped;
      });
  if (found == q->clients_.end()) {
    finishWindowPointer(false);
    return true;
  }
  auto *client = found->get();
  const auto publishGeometry = [this] {
    const auto now = std::chrono::steady_clock::now();
    if (!pointerPublishTimeValid ||
        now - pointerPublishTime >= std::chrono::milliseconds(33)) {
      q->publishWindowLayout();
      pointerPublishTime = now;
      pointerPublishTimeValid = true;
    }
  };
  const auto configureDragged = [this, client]() {
    const auto snapshot = q->windowLayout_->snapshot(q->workspace_);
    for (const auto &slot : snapshot.columns) {
      bool ownsWindow = slot.window == static_cast<LayoutWindowId>(client->id);
      if (!ownsWindow) {
        for (const auto &member : jsonArray(slot.metadata, "members"))
          if (jsonAsInteger(member) == client->id) {
            ownsWindow = true;
            break;
          }
      }
      if (!ownsWindow)
        continue;
      q->configure(client, slot.geometry);
      return true;
    }
    return false;
  };
  const PointF current(cursor->x, cursor->y);
  const Point delta = (current - pointerLast).toPoint();
  pointerLast = current;
  const Size minimum =
      clientMinimumSize(*client).expandedTo(WindowTuning::minimumWindowSize);
  if (client->state.floating) {
    const auto area = q->workArea();
    auto geometry =
        pointerResize
            ? resizedGeometry(pointerStartGeometry, pointerResizeEdges,
                              (current - pointerOrigin).toPoint(), minimum, area)
            : pointerStartGeometry.translated((current - pointerOrigin).toPoint());
    geometry.setSize(geometry.size().boundedTo(area.size()));
    geometry.moveLeft(std::clamp(geometry.x(), area.left(), area.right() - geometry.width() + 1));
    geometry.moveTop(std::clamp(geometry.y(), area.top(), area.bottom() - geometry.height() + 1));
    client->layout.manualGeometry = geometry;
    client->layout.preferredFloatingSize = geometry.size();
    q->configure(client, geometry);
    publishGeometry();
    return true;
  }
  const auto snapshot = q->windowLayout_->snapshot(q->workspace_);
  const auto updateSwapTarget = [this, &snapshot, &current] {
    pointerTarget = 0;
    pointerEdge = 0;
    Json hint = Json::object();
    for (int index = snapshot.columns.size() - 1; index >= 0; --index) {
      const auto &slot = snapshot.columns[index];
      if (slot.window == static_cast<LayoutWindowId>(pointerWindow) ||
          slot.minimized || !slot.geometry.contains(current.toPoint()))
        continue;
      pointerTarget = static_cast<int>(slot.window);
      // Direct titlebar dragging means exchange. Modifier-assisted dragging
      // keeps the older top/bottom insertion gesture for grouped tiling.
      if (!pointerClientGrab) {
        const int edge = std::min(24, slot.geometry.height() / 4);
        pointerEdge =
            current.y() < slot.geometry.top() + edge       ? -1
            : current.y() > slot.geometry.bottom() - edge ? 1
                                                           : 0;
      }
      hint = {{"target", pointerTarget},
              {"edge", pointerEdge},
              {"x", slot.geometry.x()},
              {"y", slot.geometry.y()},
              {"width", slot.geometry.width()},
              {"height", slot.geometry.height()}};
      break;
    }
    q->windowSwitcher_->setDrag(hint);
  };
  if (freeform(q->windowTemplate_, *client)) {
    const auto area = q->workArea();
    if (pointerRestore) {
      if (std::hypot(current.x() - pointerOrigin.x(),
                     current.y() - pointerOrigin.y()) <
          WindowTuning::restoreDragDistance)
        return true;
      // Back to its own size, the pointer as far across it as it was.
      const Rect from = client->layout.geometry;
      const auto *slot = slotOf(snapshot, *client);
      Size size = slot ? slot->geometry.size() : from.size();
      if (slot && jsonInt(slot->metadata, "tile")) {
        size = Size(jsonInt(slot->metadata, "restoreWidth"),
                    jsonInt(slot->metadata, "restoreHeight"));
        performWindowLayoutAction(*q->windowLayout_, *q->windowTemplate_, "tile",
                                  {{"window", client->id}, {"edges", 0}});
      }
      if (client->state.maximized)
        q->setMaximized(client, false);
      if (!size.isValid())
        size = from.size();
      const double across =
          (current.x() - from.x()) / std::max(1, from.width());
      const int left = static_cast<int>(std::lround(current.x() - across * size.width()));
      const int top = std::clamp(from.y(), static_cast<int>(current.y()) - size.height() + 1,
                                 static_cast<int>(current.y()));
      pointerStartGeometry = Rect(left, top, size.width(), size.height());
      pointerOrigin = current;
      pointerRestore = false;
      performWindowLayoutAction(
          *q->windowLayout_, *q->windowTemplate_, "place",
          {{"window", client->id}, {"x", left}, {"y", top},
           {"width", size.width()}, {"height", size.height()}});
      q->arrange();
      return true;
    }
    Rect target;
    if (pointerResize) {
      target = resizedGeometry(pointerStartGeometry, pointerResizeEdges,
                               (current - pointerOrigin).toPoint(), minimum, area);
    } else {
      std::vector<Rect> others;
      for (const auto &slot : snapshot.columns)
        if (slot.window != static_cast<LayoutWindowId>(client->id) &&
            !slot.minimized && slot.geometry.isValid())
          others.push_back(slot.geometry);
      target = snapped(pointerStartGeometry.translated(
                           (current - pointerOrigin).toPoint()),
                       area, others, WindowTuning::snapDistance);
      const auto edges = quickTileEdges(current, outputSize());
      showQuickTile(edges, edges == edgeTop ? area : quickTileRect(area, edges),
                    client);
    }
    performWindowLayoutAction(
        *q->windowLayout_, *q->windowTemplate_, "place",
        {{"window", client->id}, {"x", target.x()}, {"y", target.y()},
         {"width", target.width()}, {"height", target.height()}});
    if (configureDragged())
      publishGeometry();
    else
      q->arrange();
    return true;
  }
  if (pointerResize) {
    for (const auto &slot : snapshot.columns) {
      bool sameGroup = false;
      for (const auto &id : jsonArray(slot.metadata, "members"))
        if (jsonAsInteger(id) == client->id) {
          sameGroup = true;
          break;
        }
      if (!sameGroup)
        continue;
      for (const auto &member : q->clients_)
        if (member->id == static_cast<int>(slot.window))
          q->setMaximized(member.get(), false);
    }
    performWindowLayoutAction(
        *q->windowLayout_, *q->windowTemplate_, "resize-width",
        {{"window", client->id},
         {"width", std::clamp(client->layout.geometry.width() + delta.x(), 120,
                              std::max(120, q->workArea().width()))}});
    performWindowLayoutAction(
        *q->windowLayout_, *q->windowTemplate_, "resize-height",
        {{"window", client->id},
         {"height", client->layout.geometry.height() + delta.y()}});
    q->arrange();
    return true;
  }
  const int active =
      std::count_if(snapshot.columns.begin(), snapshot.columns.end(),
                    [](const auto &c) { return !c.minimized; });
  // A lone tiled window has nowhere to go.
  if (active == 1)
    return true;
  // Tiled: the window follows the pointer above the others; dropped, it
  // springs into its (possibly new) slot.
  if (client->scene.tree) {
    if (!pointerFollowing) {
      if (q->windowAnimations_)
        q->windowAnimations_->cancel(client->scene.tree);
      q->raiseWithDialogs(client);
      pointerFollowing = true;
    }
    auto &node = client->scene.tree->node;
    wlr_scene_node_set_position(&node, node.x + delta.x(), node.y + delta.y());
  }
  updateSwapTarget();
  return true;
}

void WaylandCompositor::Impl::finishWindowPointer(bool apply) {
  const int moving = pointerWindow, target = pointerTarget, edge = pointerEdge;
  const bool directMove = pointerClientGrab;
  const auto quickTile = pointerQuickTile;
  pointerTarget = pointerEdge = 0;
  pointerRestore = false;
  showQuickTile(0, {}, nullptr);
  for (const auto &client : q->clients_)
    if (client->id == moving) {
      client->layout.manualResize = false;
      if (clientToplevel(*client))
        wlr_xdg_toplevel_set_resizing(clientToplevel(*client), false);
    }
  if (apply && !pointerResize && target && q->windowTemplate_) {
    if (edge && !directMove)
      performWindowLayoutAction(
          *q->windowLayout_, *q->windowTemplate_, "insert-beside",
          {{"window", moving}, {"target", target}, {"after", edge > 0}});
    else
      performWindowLayoutAction(*q->windowLayout_, *q->windowTemplate_, "swap",
                                {{"window", moving}, {"target", target}});
  }
  // Dropped at a screen edge: a stacking window fills that half or quarter,
  // or the screen from the top edge, springing there.
  if (apply && quickTile && !pointerResize) {
    if (auto *client = q->clientById(moving); client && quickTile == edgeTop)
      q->setMaximized(client, true);
    else
      performWindowLayoutAction(*q->windowLayout_, *q->windowTemplate_, "tile",
                                {{"window", moving}, {"edges", quickTile}});
  }
  pointerResize = pointerClientGrab = pointerFollowing = false;
  pointerButton = pointerResizeEdges = 0;
  pointerStartGeometry = {};
  pointerPublishTimeValid = false;
  q->windowSwitcher_->setDrag({});
  // Release the interactive geometry override before arranging. Dropped,
  // swapped and regrouped windows then use the normal layout transition.
  pointerWindow = 0;
  q->arrange();
  q->publishWindowLayout();
  setCursorImage("default");
}
} // namespace LunaDash
