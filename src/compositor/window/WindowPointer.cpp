#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/window/WindowRules.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include <QJsonArray>
#include <algorithm>
#include <linux/input-event-codes.h>

namespace LunaDash {
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
  pointerLast = QPointF(cursor->x, cursor->y);
  pointerTarget = 0;
  pointerEdge = 0;
  client->manualResize = true;
  wlr_seat_pointer_notify_clear_focus(seat);
  wlr_cursor_set_xcursor(cursor, cursorManager,
                         pointerResize ? "se-resize" : "grabbing");
  return true;
}

bool WaylandCompositor::Impl::beginClientWindowPointer(ClientWindow *client,
                                                   uint32_t serial,
                                                   uint32_t edges) {
  if (!client || !q->windowTemplate_ ||
      !windowAllowsClientMoveResize(*q->windowTemplate_, *client) ||
      pointerWindow ||
      !wlr_seat_validate_pointer_grab_serial(seat, client->surface->surface,
                                             serial))
    return false;
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
  pointerLast = QPointF(cursor->x, cursor->y);
  pointerTarget = pointerEdge = 0;
  client->manualResize = true;
  wlr_xdg_toplevel_set_resizing(client->toplevel, pointerResize);
  wlr_cursor_set_xcursor(cursor, cursorManager,
                         pointerResize ? "se-resize" : "grabbing");
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
  const QPointF current(cursor->x, cursor->y);
  const QPoint delta = (current - pointerLast).toPoint();
  pointerLast = current;
  const auto snapshot = q->windowLayout_->snapshot(q->workspace_);
  const bool freeform =
      q->windowTemplate_ && q->windowTemplate_->allowOverlap &&
      q->windowTemplate_->clientMoveResize;
  if (pointerResize && freeform) {
    const auto geometry = client->geometry;
    const auto area = q->workArea();
    const bool left = pointerResizeEdges & 4u, right = pointerResizeEdges & 8u;
    const bool top = pointerResizeEdges & 1u, bottom = pointerResizeEdges & 2u;
    const int availableWidth =
        std::max(1, left ? geometry.right() - area.left() + 1
                         : area.right() - geometry.left() + 1);
    const int availableHeight =
        std::max(1, top ? geometry.bottom() - area.top() + 1
                        : area.bottom() - geometry.top() + 1);
    const int width = std::clamp(geometry.width() + (left    ? -delta.x()
                                                     : right ? delta.x()
                                                             : 0),
                                 std::min(120, availableWidth), availableWidth);
    const int height =
        std::clamp(geometry.height() + (top      ? -delta.y()
                                        : bottom ? delta.y()
                                                 : 0),
                   std::min(80, availableHeight), availableHeight);
    // Move first when growing a top/left edge, resize first when shrinking.
    const QPoint shift(left ? geometry.width() - width : 0,
                       top ? geometry.height() - height : 0);
    const QJsonObject areaJson{{"x", area.x()},
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
    q->arrange();
    return true;
  }
  if (pointerResize) {
    for (const auto &slot : snapshot.columns) {
      bool sameGroup = false;
      for (const auto &id : slot.metadata.value("members").toArray())
        if (id.toInteger() == client->id) {
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
         {"area", QJsonObject{{"x", area.x()},
                              {"y", area.y()},
                              {"width", area.width()},
                              {"height", area.height()}}}});
    q->arrange();
    return true;
  }
  pointerTarget = 0;
  QJsonObject hint;
  for (const auto &slot : snapshot.columns) {
    if (slot.window == static_cast<LayoutWindowId>(pointerWindow) ||
        slot.minimized || !slot.geometry.contains(current.toPoint()))
      continue;
    pointerTarget = static_cast<int>(slot.window);
    const int edge = std::min(24, slot.geometry.height() / 4);
    pointerEdge = current.y() < slot.geometry.top() + edge      ? -1
                  : current.y() > slot.geometry.bottom() - edge ? 1
                                                                : 0;
    hint = {
        {"target", pointerTarget},        {"edge", pointerEdge},
        {"x", slot.geometry.x()},         {"y", slot.geometry.y()},
        {"width", slot.geometry.width()}, {"height", slot.geometry.height()}};
    break;
  }
  q->windowSwitcher_->setDrag(hint);
  return true;
}

void WaylandCompositor::Impl::finishWindowPointer(bool apply) {
  const int moving = pointerWindow, target = pointerTarget, edge = pointerEdge;
  pointerTarget = pointerEdge = 0;
  for (const auto &client : q->clients_)
    if (client->id == moving) {
      client->manualResize = false;
      if (client->toplevel)
        wlr_xdg_toplevel_set_resizing(client->toplevel, false);
    }
  if (apply && !pointerResize && target && q->windowTemplate_) {
    if (edge)
      performWindowLayoutAction(
          *q->windowLayout_, *q->windowTemplate_, "insert-beside",
          {{"window", moving}, {"target", target}, {"after", edge > 0}});
    else
      performWindowLayoutAction(*q->windowLayout_, *q->windowTemplate_, "swap",
                                {{"window", moving}, {"target", target}});
  }
  pointerResize = pointerClientGrab = false;
  pointerButton = pointerResizeEdges = 0;
  q->windowSwitcher_->setDrag({});
  // Release the interactive geometry override before arranging. Dropped,
  // swapped and regrouped windows then use the normal layout transition.
  pointerWindow = 0;
  q->arrange();
  q->publishWindowLayout();
  wlr_cursor_set_xcursor(cursor, cursorManager, "default");
}
} // namespace LunaDash
