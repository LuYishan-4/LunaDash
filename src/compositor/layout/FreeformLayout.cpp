#include "compositor/layout/FreeformLayout.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace LunaDash {
Rect quickTileRect(Rect area, std::uint32_t edges) {
  constexpr int split = 8;
  const bool horizontal = edges & 12u, vertical = edges & 3u;
  const int width = horizontal ? (area.width() - split) / 2 : area.width();
  const int height = vertical ? (area.height() - split) / 2 : area.height();
  return Rect(edges & 8u ? area.x() + area.width() - width : area.x(),
              edges & 2u ? area.y() + area.height() - height : area.y(), width,
              height);
}
namespace {
// Where a new window of `size` covers the others least, like KWin's smart
// placement: the centre of `area` when it is free, otherwise beside or below
// a window or on a coarse grid, the one nearest the centre among equals.
Rect leastOverlapping(Size size, Rect area, const std::vector<Rect> &others) {
  const auto overlap = [&](const Rect &candidate) {
    long long covered = 0;
    for (const auto &other : others) {
      const Rect shared = candidate.intersected(other);
      covered += static_cast<long long>(shared.width()) * shared.height();
    }
    return covered;
  };
  const int maximumX = area.x() + area.width() - size.width();
  const int maximumY = area.y() + area.height() - size.height();
  std::vector<int> xs{area.x(), maximumX};
  std::vector<int> ys{area.y(), maximumY};
  for (const auto &other : others) {
    xs.push_back(other.x() + other.width());
    xs.push_back(other.x() - size.width());
    ys.push_back(other.y() + other.height());
    ys.push_back(other.y() - size.height());
  }
  constexpr int grid = 48;
  for (int x = area.x(); x < maximumX; x += grid)
    xs.push_back(x);
  for (int y = area.y(); y < maximumY; y += grid)
    ys.push_back(y);
  Rect best(Point(), size);
  best.moveCenter(area.center());
  long long bestOverlap = overlap(best);
  const auto distance = [&](const Rect &candidate) {
    const long long dx = candidate.center().x() - area.center().x();
    const long long dy = candidate.center().y() - area.center().y();
    return dx * dx + dy * dy;
  };
  long long bestDistance = 0;
  for (const int x : xs)
    for (const int y : ys) {
      if (bestOverlap == 0 && bestDistance == 0)
        return best;
      if (x < area.x() || x > std::max(area.x(), maximumX) || y < area.y() ||
          y > std::max(area.y(), maximumY))
        continue;
      const Rect candidate(Point(x, y), size);
      const long long covered = overlap(candidate);
      const long long away = distance(candidate);
      if (covered < bestOverlap || (covered == bestOverlap && away < bestDistance)) {
        best = candidate;
        bestOverlap = covered;
        bestDistance = away;
      }
    }
  return best;
}
} // namespace

std::vector<Rect> windowGrid(const std::vector<Rect> &windows, Rect area,
                             int gap) {
  const int count = static_cast<int>(windows.size());
  std::vector<Rect> cells(windows.size());
  if (count == 0)
    return cells;
  const int columns = static_cast<int>(std::ceil(std::sqrt(count)));
  const int rows = (count + columns - 1) / columns;
  std::vector<int> order(windows.size());
  for (int index = 0; index < count; ++index)
    order[index] = index;
  std::ranges::sort(order, [&](int a, int b) {
    return windows[a].center().y() < windows[b].center().y();
  });
  for (int row = 0; row < rows; ++row)
    std::sort(order.begin() + row * columns,
              order.begin() + std::min(count, (row + 1) * columns),
              [&](int a, int b) {
                return windows[a].center().x() < windows[b].center().x();
              });
  const double cellWidth = (area.width() - gap * (columns + 1.0)) / columns;
  const double cellHeight = (area.height() - gap * (rows + 1.0)) / rows;
  for (int slot = 0; slot < count; ++slot) {
    const int row = slot / columns;
    const int inRow = std::min(columns, count - row * columns);
    const double rowLeft =
        area.x() + (area.width() - inRow * cellWidth - (inRow + 1.0) * gap) / 2.0;
    cells[order[slot]] = Rect(
        static_cast<int>(std::lround(rowLeft + gap + (slot % columns) * (cellWidth + gap))),
        static_cast<int>(std::lround(area.y() + gap + row * (cellHeight + gap))),
        std::max(1, static_cast<int>(std::lround(cellWidth))),
        std::max(1, static_cast<int>(std::lround(cellHeight))));
  }
  return cells;
}

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
    // Quick tiling: the screen edges it fills (xdg edges: top 1, bottom 2,
    // left 4, right 8), and the size it goes back to.
    std::uint32_t tile = 0;
    Rect restore;
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
  if (action == "place")
    return place(window, Rect(jsonInt(payload, "x"), jsonInt(payload, "y"),
                              jsonInt(payload, "width"),
                              jsonInt(payload, "height")));
  if (action == "tile")
    return tile(window, static_cast<std::uint32_t>(jsonInt(payload, "edges")));
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
bool FreeformLayout::place(LayoutWindowId window, Rect geometry) {
  if (!d->entries.contains(window) || !geometry.isValid())
    return false;
  auto &entry = d->entries[window];
  entry.tile = 0;
  entry.geometry = Impl::bounded(geometry, d->area(entry.workspace));
  return true;
}
bool FreeformLayout::tile(LayoutWindowId window, std::uint32_t edges) {
  if (!d->entries.contains(window) || (edges & 3u) == 3u ||
      (edges & 12u) == 12u || (edges & ~15u))
    return false;
  auto &entry = d->entries[window];
  if (!entry.tile && entry.geometry.isValid())
    entry.restore = entry.geometry;
  if (!edges && entry.tile && entry.restore.isValid()) {
    // Back to its own size, centred where the tile was.
    Rect restored(Point(), entry.restore.size());
    restored.moveCenter(entry.geometry.center());
    entry.geometry = Impl::bounded(restored, d->area(entry.workspace));
  }
  entry.tile = edges;
  entry.maximized = false;
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
    if (entry.tile)
      entry.geometry = quickTileRect(area, entry.tile);
    if (!entry.geometry.isValid()) {
      // The default size leaves the desktop showing around a lone window.
      Size size = entry.preferred.isValid()
                      ? entry.preferred
                      : d->defaultSize.boundedTo(
                            Size(area.width() * 2 / 3, area.height() * 3 / 4));
      if (size.width() > area.width() || size.height() > area.height())
        size = size.scaledToFit(area.size());
      std::vector<Rect> others;
      for (const auto &[otherId, other] : d->entries)
        if (otherId != entry.id && other.workspace == workspace &&
            !other.minimized && other.geometry.isValid())
          others.push_back(other.geometry);
      entry.geometry = leastOverlapping(size, area, others);
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
    if (entry.tile)
      placement.metadata = {{"tile", entry.tile},
                            {"restoreWidth", entry.restore.width()},
                            {"restoreHeight", entry.restore.height()}};
    result.columns.push_back(placement);
  }
  return result;
}
} // namespace LunaDash
