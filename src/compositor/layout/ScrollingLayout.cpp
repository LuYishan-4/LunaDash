#include "compositor/layout/ScrollingLayout.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace LunaDash {
namespace {

constexpr std::size_t kMaximumColumnMembers = 8;
constexpr int kDefaultWeight = 100;

struct Member {
  LayoutWindowId window = 0;
  bool minimized = false;
  int weight = kDefaultWeight;
  Rect geometry;
};

struct Column {
  std::vector<Member> members;
  // Share of the screen width; resizing sets it from pixels.
  double proportion = 0.5;
  int x = 0;
  int width = 0;
};

struct Workspace {
  std::vector<Column> columns;
  LayoutWindowId focused = 0;
  LayoutWindowId maximized = 0;
  // Left edge of the view within the strip of columns.
  int scroll = 0;
  bool centerRequested = false;
  Rect area;
};

struct Location {
  std::size_t column = 0;
  std::size_t member = 0;
  bool found = false;
};

Location find(const Workspace &workspace, LayoutWindowId window) {
  for (std::size_t c = 0; c < workspace.columns.size(); ++c)
    for (std::size_t m = 0; m < workspace.columns[c].members.size(); ++m)
      if (workspace.columns[c].members[m].window == window)
        return {c, m, true};
  return {};
}

bool active(const Column &column) {
  return std::any_of(column.members.begin(), column.members.end(),
                     [](const Member &member) { return !member.minimized; });
}

std::size_t activeCount(const Column &column) {
  return static_cast<std::size_t>(
      std::count_if(column.members.begin(), column.members.end(),
                    [](const Member &member) { return !member.minimized; }));
}

// The first visible window of a column, preferring row `row`.
LayoutWindowId memberAt(const Column &column, int row) {
  std::vector<LayoutWindowId> visible;
  for (const auto &member : column.members)
    if (!member.minimized)
      visible.push_back(member.window);
  if (visible.empty())
    return 0;
  return visible[static_cast<std::size_t>(
      std::clamp(row, 0, static_cast<int>(visible.size()) - 1))];
}

int rowOf(const Column &column, LayoutWindowId window) {
  int row = 0;
  for (const auto &member : column.members) {
    if (member.window == window)
      return row;
    if (!member.minimized)
      ++row;
  }
  return 0;
}

// Keeps focus on a visible window after something disappeared.
void repairFocus(Workspace &workspace) {
  const auto location = find(workspace, workspace.focused);
  if (location.found &&
      !workspace.columns[location.column].members[location.member].minimized)
    return;
  workspace.focused = 0;
  const std::size_t start = location.found ? location.column : 0;
  for (std::size_t distance = 0; distance < workspace.columns.size();
       ++distance) {
    for (const auto index : {start + distance, start - distance}) {
      if (index >= workspace.columns.size())
        continue;
      if (const auto window = memberAt(workspace.columns[index], 0)) {
        workspace.focused = window;
        return;
      }
    }
  }
}

} // namespace

class ScrollingLayout::Impl {
public:
  std::pair<LayoutWorkspaceId, Workspace *> locate(LayoutWindowId window) {
    for (auto &[id, workspace] : workspaces)
      if (find(workspace, window).found)
        return {id, &workspace};
    return {0, nullptr};
  }

  // A new column right of the focused one.
  void addColumn(Workspace &workspace, Member member) {
    std::size_t index = workspace.columns.size();
    if (const auto location = find(workspace, workspace.focused);
        location.found)
      index = location.column + 1;
    Column column;
    column.proportion = proportion;
    column.members.push_back(member);
    workspace.columns.insert(workspace.columns.begin() +
                                 static_cast<std::ptrdiff_t>(index),
                             std::move(column));
  }

  void eraseEmpty(Workspace &workspace) {
    std::erase_if(workspace.columns,
                  [](const Column &column) { return column.members.empty(); });
  }

  int gap = 12;
  double proportion = 0.5;
  bool alwaysCenter = false;
  std::unordered_map<LayoutWorkspaceId, Workspace> workspaces;
};

ScrollingLayout::ScrollingLayout() : d(std::make_unique<Impl>()) {}
ScrollingLayout::~ScrollingLayout() = default;

void ScrollingLayout::configure(const Json &settings) {
  d->gap = std::clamp(jsonInt(settings, "gap", d->gap), 0, 64);
  d->proportion =
      std::clamp(jsonInt(settings, "columnWidth",
                         static_cast<int>(std::lround(d->proportion * 100))),
                 20, 100) /
      100.0;
  const auto center = jsonString(settings, "centerFocusedColumn");
  if (center == "never" || center == "always")
    d->alwaysCenter = center == "always";
}

bool ScrollingLayout::insert(LayoutWorkspaceId workspaceId,
                             LayoutWindowId window, Size) {
  if (!window || d->locate(window).second)
    return false;
  auto &workspace = d->workspaces[workspaceId];
  d->addColumn(workspace, {window, false, kDefaultWeight, {}});
  workspace.focused = window;
  return true;
}

bool ScrollingLayout::remove(LayoutWindowId window) {
  auto [workspaceId, workspace] = d->locate(window);
  if (!workspace)
    return false;
  if (workspace->maximized == window)
    workspace->maximized = 0;
  const auto location = find(*workspace, window);
  auto &members = workspace->columns[location.column].members;
  members.erase(members.begin() +
                static_cast<std::ptrdiff_t>(location.member));
  d->eraseEmpty(*workspace);
  repairFocus(*workspace);
  if (workspace->columns.empty())
    d->workspaces.erase(workspaceId);
  return true;
}

bool ScrollingLayout::setMinimized(LayoutWindowId window, bool minimized) {
  auto [workspaceId, workspace] = d->locate(window);
  (void)workspaceId;
  if (!workspace)
    return false;
  const auto location = find(*workspace, window);
  auto &member = workspace->columns[location.column].members[location.member];
  member.minimized = minimized;
  if (minimized && workspace->maximized == window)
    workspace->maximized = 0;
  if (!minimized)
    workspace->focused = window;
  repairFocus(*workspace);
  return true;
}

bool ScrollingLayout::setMaximized(LayoutWindowId window, bool maximized) {
  auto [workspaceId, workspace] = d->locate(window);
  (void)workspaceId;
  if (!workspace)
    return false;
  if (maximized) {
    workspace->maximized = window;
    workspace->focused = window;
  } else if (workspace->maximized == window) {
    workspace->maximized = 0;
  }
  return true;
}

bool ScrollingLayout::moveToWorkspace(LayoutWindowId window,
                                      LayoutWorkspaceId destinationId) {
  auto [sourceId, source] = d->locate(window);
  if (!source)
    return false;
  if (sourceId == destinationId)
    return true;
  if (source->maximized == window)
    source->maximized = 0;
  const auto location = find(*source, window);
  auto &members = source->columns[location.column].members;
  const Member member = members[location.member];
  members.erase(members.begin() +
                static_cast<std::ptrdiff_t>(location.member));
  d->eraseEmpty(*source);
  repairFocus(*source);
  if (source->columns.empty())
    d->workspaces.erase(sourceId);
  auto &destination = d->workspaces[destinationId];
  d->addColumn(destination, member);
  if (!member.minimized)
    destination.focused = window;
  repairFocus(destination);
  return true;
}

bool ScrollingLayout::focus(LayoutWindowId window) {
  auto [workspaceId, workspace] = d->locate(window);
  (void)workspaceId;
  if (!workspace)
    return false;
  const auto location = find(*workspace, window);
  if (workspace->columns[location.column].members[location.member].minimized)
    return false;
  workspace->focused = window;
  return true;
}

bool ScrollingLayout::performAction(const std::string &action,
                                    const Json &payload) {
  const auto window =
      static_cast<LayoutWindowId>(jsonInteger(payload, "window"));
  const auto target =
      static_cast<LayoutWindowId>(jsonInteger(payload, "target"));
  const int dx = jsonInt(payload, "dx");
  const int dy = jsonInt(payload, "dy");

  if (action == "focus-direction") {
    const auto found = d->workspaces.find(
        static_cast<LayoutWorkspaceId>(jsonInteger(payload, "workspace")));
    if (found == d->workspaces.end())
      return false;
    auto &workspace = found->second;
    const auto location = find(workspace, workspace.focused);
    if (!location.found)
      return false;
    const auto &current = workspace.columns[location.column];
    const int row = rowOf(current, workspace.focused);
    if (dy != 0) {
      const auto next = memberAt(current, row + dy);
      if (!next || next == workspace.focused)
        return false;
      workspace.focused = next;
      return true;
    }
    // Skip columns whose windows are all minimized.
    for (auto index = static_cast<std::ptrdiff_t>(location.column) + dx;
         dx != 0 && index >= 0 &&
         index < static_cast<std::ptrdiff_t>(workspace.columns.size());
         index += dx) {
      if (const auto next = memberAt(
              workspace.columns[static_cast<std::size_t>(index)], row)) {
        workspace.focused = next;
        return true;
      }
    }
    return false;
  }

  auto [workspaceId, workspace] = d->locate(window);
  (void)workspaceId;
  if (!workspace)
    return false;
  auto location = find(*workspace, window);

  if (action == "reorder") {
    // Moves the whole column.
    const int direction = jsonInt(payload, "direction");
    const auto other = static_cast<std::ptrdiff_t>(location.column) + direction;
    if (direction == 0 || other < 0 ||
        other >= static_cast<std::ptrdiff_t>(workspace->columns.size()))
      return false;
    std::swap(workspace->columns[location.column],
              workspace->columns[static_cast<std::size_t>(other)]);
    workspace->focused = window;
    return true;
  }
  if (action == "swap") {
    const auto other = find(*workspace, target);
    if (!other.found || window == target)
      return false;
    std::swap(workspace->columns[location.column]
                  .members[location.member]
                  .window,
              workspace->columns[other.column].members[other.member].window);
    workspace->focused = window;
    return true;
  }
  if (action == "group-direction") {
    auto &members = workspace->columns[location.column].members;
    if (dy != 0) {
      // Moves the window up or down inside its column.
      const auto other = static_cast<std::ptrdiff_t>(location.member) + dy;
      if (other < 0 || other >= static_cast<std::ptrdiff_t>(members.size()))
        return false;
      std::swap(members[location.member],
                members[static_cast<std::size_t>(other)]);
      workspace->focused = window;
      return true;
    }
    // Consumes the window into the neighbouring column.
    const auto other = static_cast<std::ptrdiff_t>(location.column) + dx;
    if (dx == 0 || other < 0 ||
        other >= static_cast<std::ptrdiff_t>(workspace->columns.size()) ||
        workspace->columns[static_cast<std::size_t>(other)].members.size() >=
            kMaximumColumnMembers)
      return false;
    const Member member = members[location.member];
    auto &destination =
        workspace->columns[static_cast<std::size_t>(other)].members;
    destination.push_back(member);
    members.erase(members.begin() +
                  static_cast<std::ptrdiff_t>(location.member));
    d->eraseEmpty(*workspace);
    workspace->focused = window;
    return true;
  }
  if (action == "group-with" || action == "insert-beside") {
    const auto other = find(*workspace, target);
    if (!other.found || window == target ||
        (other.column != location.column &&
         workspace->columns[other.column].members.size() >=
             kMaximumColumnMembers))
      return false;
    const bool after =
        action == "group-with" || jsonBool(payload, "after", true);
    const Member member =
        workspace->columns[location.column].members[location.member];
    auto &members = workspace->columns[location.column].members;
    members.erase(members.begin() +
                  static_cast<std::ptrdiff_t>(location.member));
    d->eraseEmpty(*workspace);
    const auto destination = find(*workspace, target);
    auto &targetMembers = workspace->columns[destination.column].members;
    targetMembers.insert(
        targetMembers.begin() +
            static_cast<std::ptrdiff_t>(destination.member + (after ? 1 : 0)),
        member);
    workspace->focused = window;
    return true;
  }
  if (action == "expel") {
    auto &members = workspace->columns[location.column].members;
    if (members.size() < 2)
      return false;
    Column column;
    column.proportion = workspace->columns[location.column].proportion;
    column.members.push_back(members[location.member]);
    members.erase(members.begin() +
                  static_cast<std::ptrdiff_t>(location.member));
    workspace->columns.insert(workspace->columns.begin() +
                                  static_cast<std::ptrdiff_t>(
                                      location.column + 1),
                              std::move(column));
    workspace->focused = window;
    return true;
  }
  if (action == "resize-width") {
    const int width = std::max(1, workspace->area.width());
    workspace->columns[location.column].proportion =
        std::clamp(static_cast<double>(jsonInt(payload, "width")) / width,
                   0.1, 1.0);
    workspace->focused = window;
    return true;
  }
  if (action == "resize-height") {
    auto &column = workspace->columns[location.column];
    if (activeCount(column) < 2)
      return false;
    const int height = std::max(1, workspace->area.height());
    int total = 0;
    for (const auto &member : column.members)
      if (!member.minimized)
        total += member.weight;
    auto &member = column.members[location.member];
    member.weight = std::clamp(
        static_cast<int>(std::lround(static_cast<double>(
                                         jsonInt(payload, "height")) *
                                     total / height)),
        10, 1000);
    return true;
  }
  if (action == "center") {
    workspace->focused = window;
    workspace->centerRequested = true;
    return true;
  }
  return false;
}

std::vector<WindowPlacement> ScrollingLayout::layout(LayoutWorkspaceId workspaceId,
                                                     Rect area) {
  const auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end() || !area.isValid())
    return {};
  auto &workspace = found->second;
  workspace.area = area;
  const int gap = d->gap;
  const int height = std::max(1, area.height() - 2 * gap);

  // Columns on one horizontal strip, in strip coordinates.
  int x = 0;
  std::vector<Column *> visible;
  for (auto &column : workspace.columns) {
    for (auto &member : column.members)
      member.geometry = {};
    if (!active(column))
      continue;
    column.width = std::clamp(
        static_cast<int>(std::lround(column.proportion * (area.width() - 2 * gap))),
        1, std::max(1, area.width() - 2 * gap));
    column.x = x;
    x += column.width + gap;
    visible.push_back(&column);
  }
  const int strip = std::max(0, x - gap);
  const int viewport = std::max(1, area.width() - 2 * gap);

  // Scroll so the focused column is on screen; a strip that fits is centred.
  int offset = 0;
  if (strip <= viewport) {
    offset = -(viewport - strip) / 2;
  } else {
    const auto location = find(workspace, workspace.focused);
    const Column *focused =
        location.found ? &workspace.columns[location.column] : nullptr;
    int scroll = workspace.scroll;
    if (focused && active(*focused)) {
      if (d->alwaysCenter || workspace.centerRequested)
        scroll = focused->x + focused->width / 2 - viewport / 2;
      else if (focused->x < scroll)
        scroll = focused->x;
      else if (focused->x + focused->width > scroll + viewport)
        scroll = focused->x + focused->width - viewport;
    }
    workspace.scroll = std::clamp(scroll, 0, strip - viewport);
    offset = workspace.scroll;
  }
  workspace.centerRequested = false;

  for (auto *column : visible) {
    int total = 0;
    std::size_t count = 0;
    for (const auto &member : column->members)
      if (!member.minimized) {
        total += std::max(1, member.weight);
        ++count;
      }
    const int available =
        std::max(1, height - gap * static_cast<int>(count - 1));
    int y = area.y() + gap;
    std::size_t index = 0;
    int used = 0;
    for (auto &member : column->members) {
      if (member.minimized)
        continue;
      ++index;
      const int rowHeight =
          index == count ? available - used
                         : static_cast<int>(std::lround(
                               static_cast<double>(available) *
                               std::max(1, member.weight) / total));
      member.geometry = Rect(area.x() + gap + column->x - offset, y,
                             column->width, std::max(1, rowHeight));
      y += rowHeight + gap;
      used += rowHeight;
    }
  }

  const auto placements =
      filterPlacements(workspaceId, area, snapshot(workspaceId).columns);
  for (auto &column : workspace.columns)
    for (auto &member : column.members)
      for (const auto &placement : placements)
        if (placement.window == member.window)
          member.geometry = placement.geometry;
  return snapshot(workspaceId).columns;
}

std::vector<WindowPlacement>
ScrollingLayout::presentation(LayoutWorkspaceId workspaceId, Rect area) {
  auto placements = layout(workspaceId, area);
  const auto maximized = snapshot(workspaceId).maximizedWindow;
  if (maximized)
    for (auto &placement : placements) {
      placement.hiddenByMaximize = placement.window != maximized;
      if (placement.window == maximized)
        placement.geometry = area;
    }
  return placements;
}

WorkspaceLayoutSnapshot
ScrollingLayout::snapshot(LayoutWorkspaceId workspaceId) const {
  WorkspaceLayoutSnapshot result;
  result.workspace = workspaceId;
  const auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end())
    return result;
  const auto &workspace = found->second;
  result.focusedWindow = workspace.focused;
  result.maximizedWindow = workspace.maximized;
  for (std::size_t c = 0; c < workspace.columns.size(); ++c) {
    const auto &column = workspace.columns[c];
    Json members = Json::array();
    for (const auto &member : column.members)
      members.push_back(static_cast<std::int64_t>(member.window));
    int row = 0;
    for (const auto &member : column.members) {
      WindowPlacement placement;
      placement.window = member.window;
      placement.width = column.width;
      placement.minimized = member.minimized;
      placement.focused = member.window == workspace.focused;
      placement.geometry = member.geometry;
      placement.metadata = {{"group", static_cast<int>(c)},
                            {"row", member.minimized ? -1 : row++},
                            {"members", members}};
      result.columns.push_back(placement);
    }
  }
  return result;
}

} // namespace LunaDash
