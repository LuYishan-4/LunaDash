#include "compositor/layout/FreeformLayout.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace LunaDash {
class FreeformLayout::Impl {
public:
  struct Entry {
    LayoutWindowId id = 0;
    LayoutWorkspaceId workspace = 0;
    Size preferred;
    Rect geometry;
    bool minimized = false;
    bool maximized = false;
    bool fresh = true;
  };
  std::unordered_map<LayoutWindowId, Entry> entries;
  std::vector<LayoutWindowId> order;
  std::unordered_map<LayoutWorkspaceId, LayoutWindowId> focused;
  std::unordered_map<LayoutWorkspaceId, Rect> areas;
  Size defaultSize{900, 600};
  LayoutWindowId focusedWindow(LayoutWorkspaceId workspace) const {
    const auto found = focused.find(workspace);
    return found == focused.end() ? 0 : found->second;
  }
  Rect area(LayoutWorkspaceId workspace) const {
    const auto found = areas.find(workspace);
    return found == areas.end() ? Rect{} : found->second;
  }
  void removeFromOrder(LayoutWindowId window) {
    std::erase(order, window);
  }
  static Rect bounded(Rect geometry, Rect area) {
    if (!area.isValid())
      return geometry;
    geometry.setSize(
        geometry.size().boundedTo(area.size()).expandedTo(Size(1, 1)));
    geometry.moveLeft(std::clamp(geometry.x(), area.x(),
                                 area.x() + area.width() - geometry.width()));
    geometry.moveTop(std::clamp(geometry.y(), area.y(),
                                area.y() + area.height() - geometry.height()));
    return geometry;
  }
};
FreeformLayout::FreeformLayout() : d(std::make_unique<Impl>()) {}
FreeformLayout::~FreeformLayout() = default;
void FreeformLayout::configure(const Json &settings) {
  d->defaultSize = Size(
      std::clamp(jsonInt(settings, "defaultWidth", d->defaultSize.width()),
                 240, 2400),
      std::clamp(jsonInt(settings, "defaultHeight", d->defaultSize.height()),
                 160, 1600));
}
bool FreeformLayout::insert(LayoutWorkspaceId workspace, LayoutWindowId window,
                            Size preferred) {
  if (!window || d->entries.contains(window))
    return false;
  d->entries.emplace(window, Impl::Entry{window, workspace, preferred});
  d->order.push_back(window);
  d->focused[workspace] = window;
  return true;
}
bool FreeformLayout::remove(LayoutWindowId window) {
  if (!d->entries.contains(window))
    return false;
  const auto workspace = d->entries[window].workspace;
  d->entries.erase(window);
  d->removeFromOrder(window);
  if (d->focusedWindow(workspace) == window) {
    d->focused.erase(workspace);
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
  if (minimized &&
      d->focusedWindow(d->entries[window].workspace) == window)
    focusRight(d->entries[window].workspace);
  return true;
}
bool FreeformLayout::setMaximized(LayoutWindowId window, bool maximized) {
  if (!d->entries.contains(window))
    return false;
  auto &entry = d->entries[window];
  if (maximized)
    for (auto &[id, other] : d->entries) {
      (void)id;
      if (other.workspace == entry.workspace)
        other.maximized = false;
    }
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
  if (d->focusedWindow(previous) == window) {
    d->focused.erase(previous);
    focusRight(previous);
  }
  return focus(window);
}
bool FreeformLayout::focus(LayoutWindowId window) {
  if (!d->entries.contains(window) || d->entries[window].minimized)
    return false;
  d->focused[d->entries[window].workspace] = window;
  d->removeFromOrder(window);
  d->order.push_back(window);
  return true;
}
bool FreeformLayout::focusRight(LayoutWorkspaceId workspace) {
  for (auto id : d->order)
    if (d->entries[id].workspace == workspace && !d->entries[id].minimized)
      return focus(id);
  d->focused.erase(workspace);
  return false;
}
bool FreeformLayout::focusLeft(LayoutWorkspaceId workspace) {
  for (auto it = d->order.crbegin(); it != d->order.crend(); ++it)
    if (d->entries[*it].workspace == workspace && !d->entries[*it].minimized &&
        *it != d->focusedWindow(workspace))
      return focus(*it);
  return false;
}
bool FreeformLayout::focusUp(LayoutWorkspaceId workspace) {
  return focusLeft(workspace);
}
bool FreeformLayout::focusDown(LayoutWorkspaceId workspace) {
  return focusRight(workspace);
}
bool FreeformLayout::performAction(const std::string &action,
                                  const Json &payload) {
  const auto window =
      static_cast<LayoutWindowId>(jsonInteger(payload, "window"));
  const auto target =
      static_cast<LayoutWindowId>(jsonInteger(payload, "target"));
  const auto workspace =
      static_cast<LayoutWorkspaceId>(jsonInteger(payload, "workspace"));
  const auto areaObject = jsonObject(payload, "area");
  const Rect area(jsonInt(areaObject, "x"), jsonInt(areaObject, "y"),
                   jsonInt(areaObject, "width"),
                   jsonInt(areaObject, "height"));

  if (action == "focus-direction") {
    const int dx = jsonInt(payload, "dx");
    const int dy = jsonInt(payload, "dy");
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
    return reorder(window, jsonInt(payload, "direction"));
  if (action == "resize-width")
    return resize(window, jsonInt(payload, "width"));
  if (action == "resize-height")
    return resizeHeight(window, jsonInt(payload, "height"));
  if (action == "move-by")
    return moveSingle(window,
                      Point(jsonInt(payload, "dx"),
                             jsonInt(payload, "dy")),
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
      Impl::bounded(entry.geometry, d->area(entry.workspace));
  return true;
}
bool FreeformLayout::resizeHeight(LayoutWindowId window, int height) {
  if (!d->entries.contains(window) || !d->entries[window].geometry.isValid())
    return false;
  auto &entry = d->entries[window];
  entry.geometry.setHeight(std::clamp(height, 80, 32768));
  entry.geometry =
      Impl::bounded(entry.geometry, d->area(entry.workspace));
  return true;
}
bool FreeformLayout::moveSingle(LayoutWindowId window, Point delta,
                                Rect area) {
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
  d->removeFromOrder(window);
  d->order.insert(d->order.begin(), window);
  return true;
}
bool FreeformLayout::center(LayoutWindowId window, Rect area) {
  if (!d->entries.contains(window) || !area.isValid())
    return false;
  d->entries[window].geometry.moveCenter(area.center());
  return true;
}
std::vector<WindowPlacement> FreeformLayout::layout(LayoutWorkspaceId workspace,
                                                    Rect area) {
  if (!area.isValid())
    return {};
  d->areas[workspace] = area;
  for (auto &[id, entry] : d->entries) {
    (void)id;
    if (entry.workspace != workspace || entry.minimized)
      continue;
    if (!entry.geometry.isValid()) {
      Size size =
          entry.preferred.isValid() ? entry.preferred : d->defaultSize;
      if (size.width() > area.width() || size.height() > area.height())
        size = size.scaledToFit(area.size());
      entry.geometry = Rect(Point(), size);
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
std::vector<WindowPlacement>
FreeformLayout::presentation(LayoutWorkspaceId workspace, Rect area) {
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
  result.focusedWindow = d->focusedWindow(workspace);
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
    result.columns.push_back(placement);
  }
  return result;
}
} // namespace LunaDash
