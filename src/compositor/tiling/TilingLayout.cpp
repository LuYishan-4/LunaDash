#include "compositor/tiling/TilingLayout.hpp"
#include "compositor/tiling/TilingGeometry.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LunaDash {

namespace {
constexpr int kMaximumColumns = 4096;
constexpr std::size_t kMaximumActiveMembers = 8;

struct Member {
  TilingWindowId window = 0;
  bool minimized = false;
  QRect geometry;
  int weight = 1000;
};

struct Column {
  int width = 0;
  std::vector<Member> members;
};

struct Workspace {
  std::vector<Column> columns;
  TilingWindowId focused = 0;
  TilingWindowId maximized = 0;
  int scrollOffset = 0;
  QPoint singleOffset;
  int singleHeight = 0;
  int availableHeight = 0;
};

struct Location {
  std::size_t column = 0;
  std::size_t member = 0;
  bool found = false;
};

Location findWindow(const Workspace &workspace, TilingWindowId window) {
  for (std::size_t column = 0; column < workspace.columns.size(); ++column) {
    const auto &members = workspace.columns[column].members;
    for (std::size_t member = 0; member < members.size(); ++member) {
      if (members[member].window == window)
        return {column, member, true};
    }
  }
  return {};
}

std::size_t activeCount(const Column &column) {
  return static_cast<std::size_t>(
      std::count_if(column.members.begin(), column.members.end(),
                    [](const Member &member) { return !member.minimized; }));
}

TilingWindowId firstActive(const Column &column) {
  const auto member =
      std::find_if(column.members.begin(), column.members.end(),
                   [](const Member &item) { return !item.minimized; });
  return member == column.members.end() ? 0 : member->window;
}

void selectAvailableFocus(Workspace &workspace) {
  const auto current = findWindow(workspace, workspace.focused);
  if (current.found &&
      !workspace.columns[current.column].members[current.member].minimized)
    return;
  workspace.focused = 0;
  for (const auto &column : workspace.columns) {
    if (const auto window = firstActive(column)) {
      workspace.focused = window;
      break;
    }
  }
}
} // namespace

class ScrollableTilingLayout::Impl {
public:
  Impl(int requestedDefaultWidth, int requestedGap)
      : defaultWidth(std::max(1, requestedDefaultWidth)),
        gap(std::max(0, requestedGap)) {}

  std::pair<TilingWorkspaceId, Workspace *> locate(TilingWindowId window) {
    for (auto &[id, workspace] : workspaces) {
      if (findWindow(workspace, window).found)
        return {id, &workspace};
    }
    return {0, nullptr};
  }

  std::pair<TilingWorkspaceId, const Workspace *>
  locate(TilingWindowId window) const {
    for (const auto &[id, workspace] : workspaces) {
      if (findWindow(workspace, window).found)
        return {id, &workspace};
    }
    return {0, nullptr};
  }

  int defaultWidth;
  int gap;
  std::unordered_map<TilingWorkspaceId, Workspace> workspaces;
};

ScrollableTilingLayout::ScrollableTilingLayout(int defaultWidth, int gap)
    : d(std::make_unique<Impl>(defaultWidth, gap)) {}
ScrollableTilingLayout::~ScrollableTilingLayout() = default;
ScrollableTilingLayout::ScrollableTilingLayout(
    ScrollableTilingLayout &&) noexcept = default;
ScrollableTilingLayout &
ScrollableTilingLayout::operator=(ScrollableTilingLayout &&) noexcept = default;

void ScrollableTilingLayout::setGap(int gap) { d->gap = std::max(0, gap); }

bool ScrollableTilingLayout::insert(TilingWorkspaceId workspaceId,
                                    TilingWindowId window, int width) {
  if (window == 0 || d->locate(window).second)
    return false;
  auto &workspace = d->workspaces[workspaceId];
  if (workspace.columns.size() >= kMaximumColumns)
    return false;
  const auto focused = findWindow(workspace, workspace.focused);
  const auto position =
      focused.found ? focused.column + 1 : workspace.columns.size();
  workspace.columns.insert(
      workspace.columns.begin() + static_cast<std::ptrdiff_t>(position),
      {width > 0 ? width : d->defaultWidth, {{window, false, {}}}});
  workspace.focused = window;
  return true;
}

bool ScrollableTilingLayout::remove(TilingWindowId window) {
  auto [workspaceId, workspace] = d->locate(window);
  if (!workspace)
    return false;
  if (workspace->maximized == window)
    workspace->maximized = 0;
  const auto location = findWindow(*workspace, window);
  auto &members = workspace->columns[location.column].members;
  members.erase(members.begin() + static_cast<std::ptrdiff_t>(location.member));
  if (members.empty())
    workspace->columns.erase(workspace->columns.begin() +
                             static_cast<std::ptrdiff_t>(location.column));
  selectAvailableFocus(*workspace);
  if (workspace->columns.empty())
    d->workspaces.erase(workspaceId);
  return true;
}

bool ScrollableTilingLayout::setMinimized(TilingWindowId window,
                                          bool minimized) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace)
    return false;
  const auto location = findWindow(*workspace, window);
  auto &column = workspace->columns[location.column];
  auto &member = column.members[location.member];
  if (member.minimized == minimized)
    return true;
  if (!minimized && activeCount(column) >= kMaximumActiveMembers)
    return false;
  member.minimized = minimized;
  if (minimized && workspace->maximized == window)
    workspace->maximized = 0;
  if (!minimized)
    workspace->focused = window;
  selectAvailableFocus(*workspace);
  return true;
}

bool ScrollableTilingLayout::setMaximized(TilingWindowId window,
                                          bool maximized) {
  const auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace)
    return false;
  const auto location = findWindow(*workspace, window);
  if (maximized) {
    if (workspace->columns[location.column].members[location.member].minimized)
      return false;
    if (workspace->maximized != window) {
      workspace->maximized = window;
      workspace->focused = window;
    }
  } else if (workspace->maximized == window) {
    workspace->maximized = 0;
  }
  return true;
}

bool ScrollableTilingLayout::moveToWorkspace(TilingWindowId window,
                                             TilingWorkspaceId destinationId) {
  auto [sourceId, source] = d->locate(window);
  if (!source)
    return false;
  if (sourceId == destinationId)
    return true;
  auto &destination = d->workspaces[destinationId];
  if (destination.columns.size() >= kMaximumColumns)
    return false;
  if (source->maximized == window)
    source->maximized = 0;
  const auto location = findWindow(*source, window);
  auto &sourceColumn = source->columns[location.column];
  Member member = sourceColumn.members[location.member];
  const int width = sourceColumn.width;
  sourceColumn.members.erase(sourceColumn.members.begin() +
                             static_cast<std::ptrdiff_t>(location.member));
  if (sourceColumn.members.empty())
    source->columns.erase(source->columns.begin() +
                          static_cast<std::ptrdiff_t>(location.column));
  destination.columns.push_back({width, {member}});
  if (!member.minimized)
    destination.focused = window;
  selectAvailableFocus(*source);
  if (source->columns.empty())
    d->workspaces.erase(sourceId);
  selectAvailableFocus(destination);
  return true;
}

bool ScrollableTilingLayout::focus(TilingWindowId window) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace)
    return false;
  const auto location = findWindow(*workspace, window);
  if (workspace->columns[location.column].members[location.member].minimized)
    return false;
  workspace->focused = window;
  return true;
}

bool ScrollableTilingLayout::focusLeft(TilingWorkspaceId workspaceId) {
  auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end())
    return false;
  auto &workspace = found->second;
  const auto current = findWindow(workspace, workspace.focused);
  if (!current.found)
    return false;
  for (std::size_t column = current.column; column-- > 0;) {
    if (const auto window = firstActive(workspace.columns[column])) {
      workspace.focused = window;
      return true;
    }
  }
  return false;
}

bool ScrollableTilingLayout::focusRight(TilingWorkspaceId workspaceId) {
  auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end())
    return false;
  auto &workspace = found->second;
  const auto current = findWindow(workspace, workspace.focused);
  if (!current.found)
    return false;
  for (std::size_t column = current.column + 1;
       column < workspace.columns.size(); ++column) {
    if (const auto window = firstActive(workspace.columns[column])) {
      workspace.focused = window;
      return true;
    }
  }
  return false;
}

bool ScrollableTilingLayout::focusUp(TilingWorkspaceId workspaceId) {
  auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end())
    return false;
  auto &workspace = found->second;
  const auto current = findWindow(workspace, workspace.focused);
  if (!current.found)
    return false;
  const auto &members = workspace.columns[current.column].members;
  for (std::size_t member = current.member; member-- > 0;) {
    if (!members[member].minimized) {
      workspace.focused = members[member].window;
      return true;
    }
  }
  return false;
}

bool ScrollableTilingLayout::focusDown(TilingWorkspaceId workspaceId) {
  auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end())
    return false;
  auto &workspace = found->second;
  const auto current = findWindow(workspace, workspace.focused);
  if (!current.found)
    return false;
  const auto &members = workspace.columns[current.column].members;
  for (std::size_t member = current.member + 1; member < members.size();
       ++member) {
    if (!members[member].minimized) {
      workspace.focused = members[member].window;
      return true;
    }
  }
  return false;
}

bool ScrollableTilingLayout::groupWith(TilingWindowId window,
                                       TilingWindowId target) {
  return insertBeside(window, target, true);
}

bool ScrollableTilingLayout::swapWindows(TilingWindowId window,
                                         TilingWindowId target) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace || window == target)
    return false;
  const auto a = findWindow(*workspace, window);
  const auto b = findWindow(*workspace, target);
  if (!b.found)
    return false;
  // Slot geometry and weight stay in place; only the window identities move.
  std::swap(workspace->columns[a.column].members[a.member].window,
            workspace->columns[b.column].members[b.member].window);
  std::swap(workspace->columns[a.column].members[a.member].minimized,
            workspace->columns[b.column].members[b.member].minimized);
  workspace->focused = window;
  return true;
}

bool ScrollableTilingLayout::insertBeside(TilingWindowId window,
                                          TilingWindowId target, bool after) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace || window == target)
    return false;
  auto a = findWindow(*workspace, window);
  auto b = findWindow(*workspace, target);
  if (!b.found ||
      (a.column != b.column &&
       workspace->columns[b.column].members.size() >= kMaximumActiveMembers))
    return false;
  const auto moving = workspace->columns[a.column].members[a.member];
  auto &members = workspace->columns[a.column].members;
  members.erase(members.begin() + static_cast<std::ptrdiff_t>(a.member));
  if (members.empty())
    workspace->columns.erase(workspace->columns.begin() +
                             static_cast<std::ptrdiff_t>(a.column));
  b = findWindow(*workspace, target);
  auto &destination = workspace->columns[b.column].members;
  destination.insert(destination.begin() + static_cast<std::ptrdiff_t>(
                                               b.member + (after ? 1 : 0)),
                     moving);
  workspace->focused = window;
  return true;
}

bool ScrollableTilingLayout::moveSingle(TilingWindowId window, QPoint delta,
                                        QRect area) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace)
    return false;
  int count = 0;
  for (const auto &column : workspace->columns)
    count += static_cast<int>(activeCount(column));
  if (count != 1)
    return false;
  const auto where = findWindow(*workspace, window);
  const auto &geometry =
      workspace->columns[where.column].members[where.member].geometry;
  if (!area.isValid() || !geometry.isValid())
    return false;
  const int visible =
      std::min({96, area.width(), area.height(), geometry.width()});
  const QPoint position = geometry.topLeft() + delta;
  workspace->singleOffset +=
      QPoint(std::clamp(position.x(), area.left() - geometry.width() + visible,
                        area.right() - visible) -
                 geometry.x(),
             std::clamp(position.y(), area.top(),
                        std::max(area.top(), area.bottom() - visible)) -
                 geometry.y());
  return true;
}

bool ScrollableTilingLayout::resizeHeight(TilingWindowId window, int height) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace)
    return false;
  const auto location = findWindow(*workspace, window);
  auto &members = workspace->columns[location.column].members;
  int total = 0, count = 0;
  for (const auto &member : members)
    if (!member.minimized) {
      total += member.geometry.height();
      ++count;
    }
  if (count == 1) {
    int allActive = 0;
    for (const auto &column : workspace->columns)
      allActive += static_cast<int>(activeCount(column));
    if (allActive != 1 || workspace->availableHeight <= 0)
      return false;
    workspace->singleHeight =
        std::clamp(height, std::min(48, workspace->availableHeight),
                   workspace->availableHeight);
    return true;
  }
  if (count < 2 || total < count)
    return false;
  const int minimum = std::min(48, total / count);
  height = std::clamp(height, minimum, total - minimum * (count - 1));
  const int remaining = total - height - minimum * (count - 1);
  qint64 sum = 0;
  for (const auto &member : members)
    if (!member.minimized && member.window != window)
      sum += std::max(1, member.geometry.height() - minimum);
  qint64 prefix = 0;
  int used = 0;
  for (auto &member : members) {
    if (member.minimized)
      continue;
    if (member.window == window) {
      member.weight = std::max(1, height - 1);
      continue;
    }
    prefix += std::max(1, member.geometry.height() - minimum);
    const int boundary = static_cast<int>(remaining * prefix / sum);
    member.weight = std::max(1, minimum + boundary - used - 1);
    used = boundary;
  }
  return true;
}

bool ScrollableTilingLayout::expel(TilingWindowId window) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace)
    return false;
  const auto location = findWindow(*workspace, window);
  if (workspace->columns[location.column].members.size() == 1 ||
      workspace->columns.size() >= kMaximumColumns)
    return false;
  auto &column = workspace->columns[location.column];
  const int width = column.width;
  const Member member = column.members[location.member];
  column.members.erase(column.members.begin() +
                       static_cast<std::ptrdiff_t>(location.member));
  workspace->columns.insert(
      workspace->columns.begin() +
          static_cast<std::ptrdiff_t>(location.column + 1),
      {width, {member}});
  workspace->focused = window;
  return true;
}

bool ScrollableTilingLayout::reorder(TilingWindowId window, int direction) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace || direction == 0)
    return false;
  const auto location = findWindow(*workspace, window);
  if (direction < 0) {
    if (location.column == 0)
      return false;
    std::iter_swap(workspace->columns.begin() +
                       static_cast<std::ptrdiff_t>(location.column),
                   workspace->columns.begin() +
                       static_cast<std::ptrdiff_t>(location.column - 1));
  } else {
    if (location.column + 1 >= workspace->columns.size())
      return false;
    std::iter_swap(workspace->columns.begin() +
                       static_cast<std::ptrdiff_t>(location.column),
                   workspace->columns.begin() +
                       static_cast<std::ptrdiff_t>(location.column + 1));
  }
  return true;
}

bool ScrollableTilingLayout::resize(TilingWindowId window, int width) {
  if (width <= 0)
    return false;
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace)
    return false;
  workspace->columns[findWindow(*workspace, window).column].width = width;
  return true;
}

bool ScrollableTilingLayout::center(TilingWindowId window, QRect area) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace || area.width() <= 0)
    return false;
  const auto target = findWindow(*workspace, window);
  qint64 left = 0;
  for (std::size_t i = 0; i < workspace->columns.size(); ++i) {
    const auto &column = workspace->columns[i];
    if (activeCount(column) == 0)
      continue;
    if (i == target.column) {
      const qint64 desired = left + column.width / 2 - area.width() / 2;
      workspace->scrollOffset = static_cast<int>(
          std::clamp<qint64>(desired, 0, std::numeric_limits<int>::max()));
      workspace->focused = window;
      return true;
    }
    left += static_cast<qint64>(column.width) + d->gap;
  }
  return false;
}

QList<TilingColumnSnapshot>
ScrollableTilingLayout::layout(TilingWorkspaceId workspaceId, QRect area) {
  auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end() || !area.isValid())
    return {};
  auto &workspace = found->second;
  workspace.availableHeight = area.height();
  int activeWindows = 0;
  for (const auto &column : workspace.columns)
    activeWindows += static_cast<int>(activeCount(column));
  if (activeWindows > 1) {
    workspace.singleOffset = {};
    workspace.singleHeight = 0;
  }
  std::vector<int> widths;
  std::vector<Column *> visible;
  for (auto &column : workspace.columns) {
    for (auto &member : column.members)
      member.geometry = {};
    if (activeCount(column) != 0) {
      widths.push_back(column.width);
      visible.push_back(&column);
    }
  }
  std::vector<LuDashRectangle> columns(visible.size());
  if (!visible.empty() &&
      ludash_layout_columns({area.x(), area.y(), area.width(), area.height()},
                            widths.data(), widths.size(), d->gap,
                            workspace.scrollOffset, columns.data(),
                            columns.size()) != columns.size())
    return {};
  for (std::size_t i = 0; i < visible.size(); ++i) {
    if (activeWindows == 1 && workspace.singleHeight > 0)
      columns[i].height = std::min(area.height(), workspace.singleHeight);
    auto &column = *visible[i];
    std::vector<Member *> active;
    for (auto &member : column.members)
      if (!member.minimized)
        active.push_back(&member);
    std::vector<LuDashRectangle> rows(active.size());
    std::vector<int> weights;
    for (const auto *member : active)
      weights.push_back(member->weight);
    if (ludash_layout_weighted_column_windows(
            columns[i], weights.data(), active.size(), d->gap, rows.data(),
            rows.size()) != rows.size())
      return {};
    for (std::size_t row = 0; row < active.size(); ++row)
      active[row]->geometry =
          QRect(rows[row].x, rows[row].y, rows[row].width, rows[row].height);
  }
  if (activeWindows == 1 && !workspace.singleOffset.isNull()) {
    for (auto *column : visible)
      for (auto &member : column->members)
        if (!member.minimized)
          member.geometry.translate(workspace.singleOffset);
    return snapshot(workspaceId).columns;
  }
  if (activeWindows > 1)
    workspace.singleOffset = {};
  const auto focused = findWindow(workspace, workspace.focused);
  if (focused.found) {
    std::size_t focusedVisible = visible.size();
    for (std::size_t i = 0; i < visible.size(); ++i)
      if (visible[i] == &workspace.columns[focused.column]) {
        focusedVisible = i;
        break;
      }
    qint64 desired = workspace.scrollOffset;
    if (focusedVisible < visible.size()) {
      if (columns[focusedVisible].width <= area.width()) {
        if (columns[focusedVisible].x < area.left())
          desired += columns[focusedVisible].x - area.left();
        else if (columns[focusedVisible].x + columns[focusedVisible].width >
                 area.x() + area.width())
          desired += columns[focusedVisible].x + columns[focusedVisible].width -
                     area.x() - area.width();
      } else {
        // Wider than the strip: bring the focused member into view.
        const auto &geometry =
            workspace.columns[focused.column].members[focused.member].geometry;
        if (geometry.left() < area.left())
          desired = static_cast<qint64>(workspace.scrollOffset) -
                    (area.left() - geometry.left());
        else if (geometry.right() > area.right())
          desired = static_cast<qint64>(workspace.scrollOffset) +
                    (geometry.right() - area.right());
      }
    }
    desired = std::clamp<qint64>(desired, 0, std::numeric_limits<int>::max());
    if (desired != workspace.scrollOffset) {
      workspace.scrollOffset = static_cast<int>(desired);
      return layout(workspaceId, area);
    }
  }
  return snapshot(workspaceId).columns;
}

TilingWorkspaceSnapshot
ScrollableTilingLayout::snapshot(TilingWorkspaceId workspaceId) const {
  TilingWorkspaceSnapshot result;
  result.workspace = workspaceId;
  const auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end())
    return result;
  const auto &workspace = found->second;
  result.scrollOffset = workspace.scrollOffset;
  result.focusedWindow = workspace.focused;
  result.maximizedWindow = workspace.maximized;
  for (std::size_t columnIndex = 0; columnIndex < workspace.columns.size();
       ++columnIndex) {
    const auto &column = workspace.columns[columnIndex];
    QList<TilingWindowId> members;
    members.reserve(static_cast<qsizetype>(column.members.size()));
    for (const auto &member : column.members)
      members.append(member.window);
    int rowIndex = 0;
    for (const auto &member : column.members) {
      const int row = member.minimized ? -1 : rowIndex++;
      result.columns.append({member.window, column.width, member.minimized,
                             member.window == workspace.focused,
                             member.geometry, static_cast<int>(columnIndex),
                             row, members});
    }
  }
  return result;
}

QList<TilingColumnSnapshot>
ScrollableTilingLayout::presentation(TilingWorkspaceId workspaceId,
                                     QRect area) {
  auto placements = layout(workspaceId, area);
  const auto maximized = snapshot(workspaceId).maximizedWindow;
  if (maximized) {
    for (auto &placement : placements) {
      placement.hiddenByMaximize = placement.window != maximized;
      if (placement.window == maximized)
        placement.geometry = area;
    }
  }
  return placements;
}

QList<QRect> tileRectangles(QRect area, int count, double columnRatio,
                            int gap) {
  if (count <= 0 || count > kMaximumColumns)
    return {};
  std::vector<LuDashRectangle> rectangles(static_cast<std::size_t>(count));
  const auto size = ludash_tile_rectangles(
      {area.x(), area.y(), area.width(), area.height()}, rectangles.size(),
      columnRatio, gap, rectangles.data(), rectangles.size());
  QList<QRect> result;
  result.reserve(static_cast<qsizetype>(size));
  for (std::size_t i = 0; i < size; ++i) {
    const auto &rectangle = rectangles[i];
    result.append(
        QRect(rectangle.x, rectangle.y, rectangle.width, rectangle.height));
  }
  return result;
}

} // namespace LunaDash
