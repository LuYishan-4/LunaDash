#include "compositor/layout/FreeformLayout.hpp"
#include <QHash>
#include <algorithm>

namespace LunaDash {
class FreeformLayout::Impl {
public:
  struct Entry {
    LayoutWindowId id = 0;
    LayoutWorkspaceId workspace = 0;
    QSize preferred;
    QRect geometry;
    bool minimized = false;
    bool maximized = false;
    bool fresh = true;
  };
  QHash<LayoutWindowId, Entry> entries;
  QList<LayoutWindowId> order;
  QHash<LayoutWorkspaceId, LayoutWindowId> focused;
  QHash<LayoutWorkspaceId, QRect> areas;
  QSize defaultSize{900, 600};
  static QRect bounded(QRect geometry, QRect area) {
    if (!area.isValid())
      return geometry;
    geometry.setSize(
        geometry.size().boundedTo(area.size()).expandedTo(QSize(1, 1)));
    geometry.moveLeft(std::clamp(geometry.x(), area.x(),
                                 area.x() + area.width() - geometry.width()));
    geometry.moveTop(std::clamp(geometry.y(), area.y(),
                                area.y() + area.height() - geometry.height()));
    return geometry;
  }
};
FreeformLayout::FreeformLayout() : d(std::make_unique<Impl>()) {}
FreeformLayout::~FreeformLayout() = default;
void FreeformLayout::configure(const QJsonObject &settings) {
  d->defaultSize = QSize(
      std::clamp(settings.value("defaultWidth").toInt(d->defaultSize.width()),
                 240, 2400),
      std::clamp(settings.value("defaultHeight").toInt(d->defaultSize.height()),
                 160, 1600));
}
bool FreeformLayout::insert(LayoutWorkspaceId workspace, LayoutWindowId window,
                            QSize preferred) {
  if (!window || d->entries.contains(window))
    return false;
  d->entries.insert(window, {window, workspace, preferred});
  d->order.append(window);
  d->focused[workspace] = window;
  return true;
}
bool FreeformLayout::remove(LayoutWindowId window) {
  if (!d->entries.contains(window))
    return false;
  const auto workspace = d->entries[window].workspace;
  d->entries.remove(window);
  d->order.removeAll(window);
  if (d->focused.value(workspace) == window) {
    d->focused.remove(workspace);
    for (auto id : d->order)
      if (d->entries[id].workspace == workspace && !d->entries[id].minimized)
        d->focused[workspace] = id;
  }
  return true;
}
bool FreeformLayout::setMinimized(LayoutWindowId window, bool minimized) {
  if (!d->entries.contains(window))
    return false;
  d->entries[window].minimized = minimized;
  if (minimized && d->focused.value(d->entries[window].workspace) == window)
    focusRight(d->entries[window].workspace);
  return true;
}
bool FreeformLayout::setMaximized(LayoutWindowId window, bool maximized) {
  if (!d->entries.contains(window))
    return false;
  auto &entry = d->entries[window];
  if (maximized)
    for (auto &other : d->entries)
      if (other.workspace == entry.workspace)
        other.maximized = false;
  entry.maximized = maximized;
  return true;
}
bool FreeformLayout::moveToWorkspace(LayoutWindowId window,
                                     LayoutWorkspaceId workspace) {
  if (!d->entries.contains(window))
    return false;
  auto &entry = d->entries[window];
  if (entry.workspace == workspace)
    return true;
  const auto previous = entry.workspace;
  entry.workspace = workspace;
  if (d->focused.value(previous) == window) {
    d->focused.remove(previous);
    focusRight(previous);
  }
  return focus(window);
}
bool FreeformLayout::focus(LayoutWindowId window) {
  if (!d->entries.contains(window) || d->entries[window].minimized)
    return false;
  d->focused[d->entries[window].workspace] = window;
  d->order.removeAll(window);
  d->order.append(window);
  return true;
}
bool FreeformLayout::focusRight(LayoutWorkspaceId workspace) {
  for (auto id : d->order)
    if (d->entries[id].workspace == workspace && !d->entries[id].minimized)
      return focus(id);
  d->focused.remove(workspace);
  return false;
}
bool FreeformLayout::focusLeft(LayoutWorkspaceId workspace) {
  for (auto it = d->order.crbegin(); it != d->order.crend(); ++it)
    if (d->entries[*it].workspace == workspace && !d->entries[*it].minimized &&
        *it != d->focused.value(workspace))
      return focus(*it);
  return false;
}
bool FreeformLayout::focusUp(LayoutWorkspaceId workspace) {
  return focusLeft(workspace);
}
bool FreeformLayout::focusDown(LayoutWorkspaceId workspace) {
  return focusRight(workspace);
}
bool FreeformLayout::performAction(const QString &action,
                                  const QJsonObject &payload) {
  const auto window =
      static_cast<LayoutWindowId>(payload.value("window").toInteger());
  const auto target =
      static_cast<LayoutWindowId>(payload.value("target").toInteger());
  const auto workspace =
      static_cast<LayoutWorkspaceId>(payload.value("workspace").toInteger());
  const auto areaObject = payload.value("area").toObject();
  const QRect area(areaObject.value("x").toInt(), areaObject.value("y").toInt(),
                   areaObject.value("width").toInt(),
                   areaObject.value("height").toInt());

  if (action == "focus-direction") {
    const int dx = payload.value("dx").toInt();
    const int dy = payload.value("dy").toInt();
    if (dx < 0)
      return focusLeft(workspace);
    if (dx > 0)
      return focusRight(workspace);
    if (dy < 0)
      return focusUp(workspace);
    if (dy > 0)
      return focusDown(workspace);
    return false;
  }
  if (action == "swap")
    return swapWindows(window, target);
  if (action == "reorder")
    return reorder(window, payload.value("direction").toInt());
  if (action == "resize-width")
    return resize(window, payload.value("width").toInt());
  if (action == "resize-height")
    return resizeHeight(window, payload.value("height").toInt());
  if (action == "move-by")
    return moveSingle(window,
                      QPoint(payload.value("dx").toInt(),
                             payload.value("dy").toInt()),
                      area);
  if (action == "center")
    return center(window, area);
  return false;
}
bool FreeformLayout::groupWith(LayoutWindowId, LayoutWindowId) { return false; }
bool FreeformLayout::expel(LayoutWindowId) { return false; }
bool FreeformLayout::insertBeside(LayoutWindowId, LayoutWindowId, bool) {
  return false;
}
bool FreeformLayout::swapWindows(LayoutWindowId window, LayoutWindowId target) {
  if (!d->entries.contains(window) || !d->entries.contains(target) ||
      d->entries[window].workspace != d->entries[target].workspace)
    return false;
  std::swap(d->entries[window].geometry, d->entries[target].geometry);
  return true;
}
bool FreeformLayout::resize(LayoutWindowId window, int width) {
  if (!d->entries.contains(window) || !d->entries[window].geometry.isValid())
    return false;
  auto &entry = d->entries[window];
  entry.geometry.setWidth(std::clamp(width, 120, 32768));
  entry.geometry =
      Impl::bounded(entry.geometry, d->areas.value(entry.workspace));
  return true;
}
bool FreeformLayout::resizeHeight(LayoutWindowId window, int height) {
  if (!d->entries.contains(window) || !d->entries[window].geometry.isValid())
    return false;
  auto &entry = d->entries[window];
  entry.geometry.setHeight(std::clamp(height, 80, 32768));
  entry.geometry =
      Impl::bounded(entry.geometry, d->areas.value(entry.workspace));
  return true;
}
bool FreeformLayout::moveSingle(LayoutWindowId window, QPoint delta,
                                QRect area) {
  if (!d->entries.contains(window) || !d->entries[window].geometry.isValid() ||
      !area.isValid())
    return false;
  auto &entry = d->entries[window];
  entry.geometry = Impl::bounded(entry.geometry.translated(delta), area);
  return true;
}
bool FreeformLayout::reorder(LayoutWindowId window, int direction) {
  if (!d->entries.contains(window))
    return false;
  if (direction >= 0)
    return focus(window);
  d->order.removeAll(window);
  d->order.prepend(window);
  return true;
}
bool FreeformLayout::center(LayoutWindowId window, QRect area) {
  if (!d->entries.contains(window) || !area.isValid())
    return false;
  d->entries[window].geometry.moveCenter(area.center());
  return true;
}
QList<WindowPlacement> FreeformLayout::layout(LayoutWorkspaceId workspace,
                                              QRect area) {
  if (!area.isValid())
    return {};
  d->areas[workspace] = area;
  for (auto &entry : d->entries) {
    if (entry.workspace != workspace || entry.minimized)
      continue;
    if (!entry.geometry.isValid()) {
      QSize size =
          entry.preferred.isValid() ? entry.preferred : d->defaultSize;
      if (size.width() > area.width() || size.height() > area.height())
        size.scale(area.size(), Qt::KeepAspectRatio);
      entry.geometry = QRect(QPoint(), size);
      entry.geometry.moveCenter(area.center());
    }
    entry.geometry = Impl::bounded(entry.geometry, area);
  }
  const auto placements =
      filterPlacements(workspace, area, snapshot(workspace).columns);
  for (const auto &placement : placements) {
    auto &entry = d->entries[placement.window];
    entry.geometry = placement.geometry;
    if (!entry.minimized)
      entry.fresh = false;
  }
  return snapshot(workspace).columns;
}
QList<WindowPlacement> FreeformLayout::presentation(LayoutWorkspaceId workspace,
                                                    QRect area) {
  auto result = layout(workspace, area);
  for (auto &placement : result)
    if (d->entries[placement.window].maximized)
      placement.geometry = area;
  return result;
}
WorkspaceLayoutSnapshot
FreeformLayout::snapshot(LayoutWorkspaceId workspace) const {
  WorkspaceLayoutSnapshot result;
  result.workspace = workspace;
  result.focusedWindow = d->focused.value(workspace);
  for (auto id : d->order) {
    const auto &entry = d->entries[id];
    if (entry.workspace != workspace)
      continue;
    if (entry.maximized)
      result.maximizedWindow = id;
    WindowPlacement placement;
    placement.window = id;
    placement.width = entry.geometry.width();
    placement.minimized = entry.minimized;
    placement.focused = result.focusedWindow == id;
    placement.geometry = entry.geometry;
    placement.newWindow = entry.fresh;
    result.columns.append(placement);
  }
  return result;
}
} // namespace LunaDash
