#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/Runtime.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include <algorithm>
#include <linux/input-event-codes.h>

namespace LunaDash {
bool WaylandCompositor::Impl::beginTiledPointer(uint32_t button) {
  auto *keyboard = preferredKeyboard();
  if (!keyboard || button != BTN_LEFT ||
      !(wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_ALT))
    return false;
  double sx = 0, sy = 0;
  auto *client = clientForSurface(surfaceAt(cursor->x, cursor->y, &sx, &sy));
  if (!client || client->floating || client->utility || !client->mapped)
    return false;
  q->focus(client);
  pointerWindow = client->id;
  pointerResize = wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_SHIFT;
  if (pointerResize)
    q->setMaximized(client, false);
  pointerLast = QPointF(cursor->x, cursor->y);
  pointerTarget = 0;
  pointerEdge = 0;
  client->manualResize = true;
  wlr_seat_pointer_notify_clear_focus(seat);
  wlr_cursor_set_xcursor(cursor, cursorManager,
                         pointerResize ? "se-resize" : "grabbing");
  return true;
}

bool WaylandCompositor::Impl::updateTiledPointer() {
  if (!pointerWindow)
    return false;
  const auto found = std::find_if(
      q->clients_.begin(), q->clients_.end(),
      [this](const auto &c) { return c->id == pointerWindow && c->mapped; });
  if (found == q->clients_.end()) {
    finishTiledPointer(false);
    return true;
  }
  auto *client = found->get();
  const QPointF current(cursor->x, cursor->y);
  const QPoint delta = (current - pointerLast).toPoint();
  pointerLast = current;
  const auto snapshot = q->tiling_.snapshot(q->workspace_);
  if (pointerResize) {
    for (const auto &slot : snapshot.columns) {
      if (!slot.columnMembers.contains(client->id))
        continue;
      for (const auto &member : q->clients_)
        if (member->id == static_cast<int>(slot.window))
          q->setMaximized(member.get(), false);
    }
    q->tiling_.resize(client->id,
                      std::clamp(client->geometry.width() + delta.x(), 120,
                                 std::max(120, q->workArea().width())));
    q->tiling_.resizeHeight(client->id, client->geometry.height() + delta.y());
    q->arrange();
    return true;
  }
  const int active =
      std::count_if(snapshot.columns.begin(), snapshot.columns.end(),
                    [](const auto &c) { return !c.minimized; });
  if (active == 1) {
    q->tiling_.moveSingle(client->id, delta, q->workArea());
    q->arrange();
    return true;
  }
  pointerTarget = 0;
  QJsonObject hint;
  for (const auto &slot : snapshot.columns) {
    if (slot.window == static_cast<TilingWindowId>(pointerWindow) ||
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

void WaylandCompositor::Impl::finishTiledPointer(bool apply) {
  const int moving = pointerWindow, target = pointerTarget, edge = pointerEdge;
  pointerTarget = pointerEdge = 0;
  for (const auto &client : q->clients_)
    if (client->id == moving)
      client->manualResize = false;
  if (apply && !pointerResize && target) {
    if (edge)
      q->tiling_.insertBeside(moving, target, edge > 0);
    else
      q->tiling_.swapWindows(moving, target);
  }
  pointerResize = false;
  q->windowSwitcher_->setDrag({});
  q->arrange();
  pointerWindow = 0;
  q->publishWindowLayout();
  wlr_cursor_set_xcursor(cursor, cursorManager, "default");
}
} // namespace LunaDash
