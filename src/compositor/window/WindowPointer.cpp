#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <linux/input-event-codes.h>

namespace LunaDash {
namespace {

// How far a stacking window can be zoomed out and in.
constexpr double kMinimumZoom = 0.25;
constexpr double kMaximumZoom = 3.0;

// Stacking resizes zoom: the whole window, text and all, grows or shrinks in
// proportion instead of the client laying itself out again. `start` is the
// window when the drag began, drawn at `startScale`; `moved` is the pointer's
// travel since then; xdg `edges` are top 1, bottom 2, left 4, right 8. The
// edges opposite the dragged ones stay where they are.
Rect zoomedGeometry(const Rect &start, double startScale, std::uint32_t edges,
                    Point moved, const Rect &area, double *scale) {
  const double logicalWidth = std::max(1.0, start.width() / startScale);
  const double logicalHeight = std::max(1.0, start.height() / startScale);
  const bool left = edges & 4u, right = edges & 8u;
  const bool top = edges & 1u, bottom = edges & 2u;
  const double horizontal =
      (start.width() + (left ? -moved.x() : moved.x())) / logicalWidth;
  const double vertical =
      (start.height() + (top ? -moved.y() : moved.y())) / logicalHeight;
  // A corner follows whichever direction was dragged further.
  double factor = (left || right) && (top || bottom) ? std::max(horizontal, vertical)
                  : left || right                    ? horizontal
                  : top || bottom                    ? vertical
                                                     : startScale;
  const double availableWidth =
      left ? start.right() - area.left() + 1 : area.right() - start.left() + 1;
  const double availableHeight =
      top ? start.bottom() - area.top() + 1 : area.bottom() - start.top() + 1;
  const double smallest =
      std::max({kMinimumZoom, 120.0 / logicalWidth, 80.0 / logicalHeight});
  const double largest =
      std::max(smallest, std::min({kMaximumZoom, availableWidth / logicalWidth,
                                   availableHeight / logicalHeight}));
  factor = std::clamp(factor, smallest, largest);
  const int width = static_cast<int>(std::lround(logicalWidth * factor));
  const int height = static_cast<int>(std::lround(logicalHeight * factor));
  *scale = factor;
  return Rect(left ? start.right() + 1 - width : start.left(),
              top ? start.bottom() + 1 - height : start.top(), width, height);
}

} // namespace

bool WaylandCompositor::Impl::beginWindowPointer(uint32_t button) {
  auto *keyboard = preferredKeyboard();
  if (!keyboard || button != BTN_LEFT ||
      !(wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_ALT))
    return false;
  double sx = 0, sy = 0;
  auto *client = clientForSurface(surfaceAt(cursor->x, cursor->y, &sx, &sy));
  if (!client || !q->windowTemplate_ ||
      !windowAllowsPointerInteraction(*q->windowTemplate_, *client))
    return false;
  q->focus(client);
  pointerWindow = client->id;
  pointerButton = button;
  pointerClientGrab = false;
  pointerResizeEdges = 10; // xdg-shell bottom | right
  pointerResize = wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_SHIFT;
  if (client->maximized) {
    q->setMaximized(client, false);
    q->arrange();
  }
  pointerLast = PointF(cursor->x, cursor->y);
  pointerOrigin = pointerLast;
  pointerStartScale = clientContentScale(*client);
  pointerStartGeometry = client->geometry;
  pointerPublishTimeValid = false;
  pointerTarget = 0;
  pointerEdge = 0;
  client->manualResize = true;
  wlr_seat_pointer_notify_clear_focus(seat);
  setCursorImage(pointerResize ? "se-resize" : "grabbing");
  return true;
}

bool WaylandCompositor::Impl::beginClientWindowPointer(ClientWindow *client,
                                                   uint32_t serial,
                                                   uint32_t edges,
                                                   bool validateSerial) {
  if (!client || !q->windowTemplate_ || pointerWindow || !client->wlSurface)
    return false;
  // A normal titlebar move is useful in both tiling and stacking: in tiling it
  // chooses another slot to swap with, while stacking follows the pointer and
  // swaps on drop. Resizing still obeys the active template capability.
  if (edges == 0) {
    if (!windowAllowsPointerInteraction(*q->windowTemplate_, *client))
      return false;
  } else if (!windowAllowsClientMoveResize(*q->windowTemplate_, *client)) {
    return false;
  }
  if (validateSerial) {
    if (!wlr_seat_validate_pointer_grab_serial(seat, client->wlSurface, serial))
      return false;
  } else if (!seat->pointer_state.grab_button) {
    return false;
  }
  // xdg-shell edges are top=1, bottom=2, left=4, right=8.
  if ((edges & ~15u) || (edges & 3u) == 3u || (edges & 12u) == 12u)
    return false;
  q->focus(client);
  if (client->maximized) {
    q->setMaximized(client, false);
    q->arrange();
  }
  pointerWindow = client->id;
  pointerButton = seat->pointer_state.grab_button;
  pointerClientGrab = true;
  pointerResizeEdges = edges;
  pointerResize = edges != 0;
  pointerLast = PointF(cursor->x, cursor->y);
  pointerOrigin = pointerLast;
  pointerStartScale = clientContentScale(*client);
  pointerStartGeometry = client->geometry;
  pointerPublishTimeValid = false;
  pointerTarget = pointerEdge = 0;
  client->manualResize = true;
  // A stacking window is zoomed, not resized: the client keeps its size.
  const bool zooms = q->windowTemplate_->allowOverlap &&
                     q->windowTemplate_->clientMoveResize;
  if (client->toplevel)
    wlr_xdg_toplevel_set_resizing(client->toplevel, pointerResize && !zooms);
  setCursorImage(pointerResize ? "se-resize" : "grabbing");
  return true;
}

bool WaylandCompositor::Impl::updateWindowPointer() {
  if (!pointerWindow)
    return false;
  const auto found = std::find_if(
      q->clients_.begin(), q->clients_.end(),
      [this](const auto &c) { return c->id == pointerWindow && c->mapped; });
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
  const bool freeform =
      q->windowTemplate_ && q->windowTemplate_->allowOverlap &&
      q->windowTemplate_->clientMoveResize;
  if (client->floating) {
    auto geometry = client->geometry;
    const auto area = q->workArea();
    if (pointerResize && freeform) {
      geometry = zoomedGeometry(pointerStartGeometry, pointerStartScale,
                                pointerResizeEdges,
                                (current - pointerOrigin).toPoint(), area,
                                &client->contentScale);
    } else if (pointerResize) {
      if (pointerResizeEdges & 4u) geometry.setLeft(std::min(geometry.right() - 119, geometry.left() + delta.x()));
      if (pointerResizeEdges & 8u) geometry.setRight(std::max(geometry.left() + 119, geometry.right() + delta.x()));
      if (pointerResizeEdges & 1u) geometry.setTop(std::min(geometry.bottom() - 79, geometry.top() + delta.y()));
      if (pointerResizeEdges & 2u) geometry.setBottom(std::max(geometry.top() + 79, geometry.bottom() + delta.y()));
    } else {
      geometry.translate(delta);
    }
    geometry.setSize(geometry.size().boundedTo(area.size()));
    geometry.moveLeft(std::clamp(geometry.x(), area.left(), area.right() - geometry.width() + 1));
    geometry.moveTop(std::clamp(geometry.y(), area.top(), area.bottom() - geometry.height() + 1));
    client->manualGeometry = geometry;
    client->preferredFloatingSize = geometry.size();
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
  if (pointerResize && freeform) {
    const auto geometry = client->geometry;
    const auto area = q->workArea();
    const auto target = zoomedGeometry(pointerStartGeometry, pointerStartScale,
                                       pointerResizeEdges,
                                       (current - pointerOrigin).toPoint(), area,
                                       &client->contentScale);
    const int width = target.width();
    const int height = target.height();
    // Move first when growing a top/left edge, resize first when shrinking.
    const Point shift(target.x() - geometry.x(), target.y() - geometry.y());
    const Json areaJson{{"x", area.x()},
                               {"y", area.y()},
                               {"width", area.width()},
                               {"height", area.height()}};
    performWindowLayoutAction(
        *q->windowLayout_, *q->windowTemplate_, "move-by",
        {{"window", client->id},
         {"dx", std::min(0, shift.x())},
         {"dy", std::min(0, shift.y())},
         {"area", areaJson}});
    performWindowLayoutAction(*q->windowLayout_, *q->windowTemplate_,
                              "resize-width",
                              {{"window", client->id}, {"width", width}});
    performWindowLayoutAction(*q->windowLayout_, *q->windowTemplate_,
                              "resize-height",
                              {{"window", client->id}, {"height", height}});
    performWindowLayoutAction(
        *q->windowLayout_, *q->windowTemplate_, "move-by",
        {{"window", client->id},
         {"dx", std::max(0, shift.x())},
         {"dy", std::max(0, shift.y())},
         {"area", areaJson}});
    if (!configureDragged())
      q->arrange();
    else
      publishGeometry();
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
         {"width", std::clamp(client->geometry.width() + delta.x(), 120,
                              std::max(120, q->workArea().width()))}});
    performWindowLayoutAction(
        *q->windowLayout_, *q->windowTemplate_, "resize-height",
        {{"window", client->id},
         {"height", client->geometry.height() + delta.y()}});
    q->arrange();
    return true;
  }
  const int active =
      std::count_if(snapshot.columns.begin(), snapshot.columns.end(),
                    [](const auto &c) { return !c.minimized; });
  if (active == 1 || freeform) {
    const auto area = q->workArea();
    performWindowLayoutAction(
        *q->windowLayout_, *q->windowTemplate_, "move-by",
        {{"window", client->id},
         {"dx", delta.x()},
         {"dy", delta.y()},
         {"area", Json::object({{"x", area.x()},
                              {"y", area.y()},
                              {"width", area.width()},
                              {"height", area.height()}})}});
    if (freeform && configureDragged()) {
      if (pointerClientGrab)
        updateSwapTarget();
      else
        q->windowSwitcher_->setDrag({});
      publishGeometry();
    } else {
      q->arrange();
    }
    return true;
  }
  // Tiled: the window follows the pointer above the others; dropped, it
  // springs into its (possibly new) slot.
  if (client->sceneTree) {
    if (!pointerFollowing) {
      if (q->windowAnimations_)
        q->windowAnimations_->cancel(client->sceneTree);
      q->raiseWithDialogs(client);
      pointerFollowing = true;
    }
    auto &node = client->sceneTree->node;
    wlr_scene_node_set_position(&node, node.x + delta.x(), node.y + delta.y());
  }
  updateSwapTarget();
  return true;
}

void WaylandCompositor::Impl::finishWindowPointer(bool apply) {
  const int moving = pointerWindow, target = pointerTarget, edge = pointerEdge;
  const bool directMove = pointerClientGrab;
  pointerTarget = pointerEdge = 0;
  for (const auto &client : q->clients_)
    if (client->id == moving) {
      client->manualResize = false;
      if (client->toplevel)
        wlr_xdg_toplevel_set_resizing(client->toplevel, false);
    }
  if (apply && !pointerResize && target && q->windowTemplate_) {
    if (directMove && q->windowTemplate_->allowOverlap &&
        pointerStartGeometry.isValid()) {
      const auto snapshot = q->windowLayout_->snapshot(q->workspace_);
      const auto current = std::find_if(
          snapshot.columns.cbegin(), snapshot.columns.cend(),
          [moving](const auto &slot) {
            return slot.window == static_cast<LayoutWindowId>(moving);
          });
      if (current != snapshot.columns.cend()) {
        const auto area = q->workArea();
        const Point delta =
            pointerStartGeometry.topLeft() - current->geometry.topLeft();
        performWindowLayoutAction(
            *q->windowLayout_, *q->windowTemplate_, "move-by",
            {{"window", moving},
             {"dx", delta.x()},
             {"dy", delta.y()},
             {"area", Json::object({{"x", area.x()},
                                  {"y", area.y()},
                                  {"width", area.width()},
                                  {"height", area.height()}})}});
      }
    }
    if (edge && !directMove)
      performWindowLayoutAction(
          *q->windowLayout_, *q->windowTemplate_, "insert-beside",
          {{"window", moving}, {"target", target}, {"after", edge > 0}});
    else
      performWindowLayoutAction(*q->windowLayout_, *q->windowTemplate_, "swap",
                                {{"window", moving}, {"target", target}});
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
