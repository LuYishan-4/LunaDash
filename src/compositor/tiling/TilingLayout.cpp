#include "compositor/tiling/TilingLayout.hpp"
#include "compositor/tiling/TilingGeometry.h"

#include <algorithm>
#include <QJsonArray>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LunaDash {

namespace {
constexpr int kMaximumColumns = 4096;
constexpr std::size_t kMaximumActiveMembers = 8;

struct Member {
  LayoutWindowId window = 0;
  bool minimized = false;
  QRect geometry;
  int weight = 1000;
};

struct Split {
  quint64 column = 0;
  bool vertical = true;
  int ratio = 500000;
  QRect geometry;
  std::unique_ptr<Split> first;
  std::unique_ptr<Split> second;
};

struct Column {
  int width = 0;
  std::vector<Member> members;
  quint64 id = 0;
  QRect geometry;
};

struct Workspace {
  std::vector<Column> columns;
  LayoutWindowId focused = 0;
  LayoutWindowId maximized = 0;
  std::unique_ptr<Split> root;
  quint64 nextColumn = 1;
  QRect area{0, 0, 1440, 900};
  QRect singleGeometry;
  QSize preferredSize{960, 640};
};

struct Location {
  std::size_t column = 0;
  std::size_t member = 0;
  bool found = false;
};

Location findWindow(const Workspace &workspace, LayoutWindowId window) {
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

LayoutWindowId firstActive(const Column &column) {
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
Column *columnById(Workspace &workspace, quint64 id) {
  for (auto &column : workspace.columns)
    if (column.id == id)
      return &column;
  return nullptr;
}

QSize minimumSize(Workspace &workspace, const Split *node) {
  if (!node)
    return {};
  if (!node->first) {
    const auto *column = columnById(workspace, node->column);
    const int count = column ? static_cast<int>(activeCount(*column)) : 0;
    return count ? QSize(1, count) : QSize();
  }
  const auto a = minimumSize(workspace, node->first.get());
  const auto b = minimumSize(workspace, node->second.get());
  if (a.isEmpty())
    return b;
  if (b.isEmpty())
    return a;
  return node->vertical
             ? QSize(a.width() + b.width(), std::max(a.height(), b.height()))
             : QSize(std::max(a.width(), b.width()), a.height() + b.height());
}

bool placeSplits(Workspace &workspace, Split *node, QRect area, int gap) {
  if (!node)
    return true;
  node->geometry = area;
  if (!node->first) {
    if (auto *column = columnById(workspace, node->column)) {
      column->geometry = area;
      column->width = area.width();
    }
    return true;
  }
  const auto a = minimumSize(workspace, node->first.get());
  const auto b = minimumSize(workspace, node->second.get());
  if (a.isEmpty())
    return placeSplits(workspace, node->second.get(), area, gap);
  if (b.isEmpty())
    return placeSplits(workspace, node->first.get(), area, gap);
  LuDashRectangle halves[2];
  if (!ludash_split_rectangle({area.x(), area.y(), area.width(), area.height()},
                              node->vertical, node->ratio, gap,
                              node->vertical ? a.width() : a.height(),
                              node->vertical ? b.width() : b.height(), halves))
    return false;
  const auto rect = [](LuDashRectangle r) {
    return QRect(r.x, r.y, r.width, r.height);
  };
  return placeSplits(workspace, node->first.get(), rect(halves[0]), gap) &&
         placeSplits(workspace, node->second.get(), rect(halves[1]), gap);
}

void largestLeaf(Workspace &workspace, Split *node, Split *&best,
                 qint64 &size) {
  if (!node)
    return;
  if (node->first) {
    largestLeaf(workspace, node->first.get(), best, size);
    largestLeaf(workspace, node->second.get(), best, size);
    return;
  }
  const auto *column = columnById(workspace, node->column);
  if (!column || !activeCount(*column))
    return;
  const qint64 candidate =
      qint64(node->geometry.width()) * node->geometry.height();
  // Equal-sized slots choose the later leaf in visual traversal order.
  if (candidate >= size) {
    best = node;
    size = candidate;
  }
}

Split *focusedLeaf(Split *node, quint64 column, bool nextVertical,
                    bool &vertical) {
  if (!node)
    return nullptr;
  if (!node->first) {
    if (node->column != column)
      return nullptr;
    vertical = nextVertical;
    return node;
  }
  if (auto *leaf = focusedLeaf(node->first.get(), column, !node->vertical,
                               vertical))
    return leaf;
  return focusedLeaf(node->second.get(), column, !node->vertical, vertical);
}

bool splitFits(Workspace &workspace, const Split *node, bool vertical, int gap,
               int minimumWidth, int minimumHeight) {
  if (!node || node->first)
    return false;
  const auto *column = columnById(workspace, node->column);
  const int count = column ? static_cast<int>(activeCount(*column)) : 0;
  if (!count)
    return false;
  const int existingHeight = minimumHeight * count + gap * (count - 1);
  return vertical
      ? node->geometry.width() >= minimumWidth * 2 + gap &&
            node->geometry.height() >= existingHeight
      : node->geometry.width() >= minimumWidth &&
            node->geometry.height() >= existingHeight + minimumHeight + gap;
}

void largestFittingLeaf(Workspace &workspace, Split *node, Split *&best,
                        bool &vertical, qint64 &size, int gap,
                        int minimumWidth, int minimumHeight) {
  if (!node)
    return;
  if (node->first) {
    largestFittingLeaf(workspace, node->first.get(), best, vertical, size, gap,
                       minimumWidth, minimumHeight);
    largestFittingLeaf(workspace, node->second.get(), best, vertical, size, gap,
                       minimumWidth, minimumHeight);
    return;
  }
  bool axis = node->geometry.width() >= node->geometry.height();
  if (!splitFits(workspace, node, axis, gap, minimumWidth, minimumHeight)) {
    axis = !axis;
    if (!splitFits(workspace, node, axis, gap, minimumWidth, minimumHeight))
      return;
  }
  const qint64 candidate = qint64(node->geometry.width()) * node->geometry.height();
  if (candidate >= size) {
    best = node;
    vertical = axis;
    size = candidate;
  }
}

void addColumn(Workspace &workspace, Member member, int gap, bool splitFocused,
                bool firstWindowOnRight, int minimumWidth, int minimumHeight) {
  placeSplits(workspace, workspace.root.get(), workspace.area, gap);
  const quint64 id = workspace.nextColumn++;
  Split *target = nullptr;
  bool vertical = true;
  if (splitFocused) {
    const auto focus = findWindow(workspace, workspace.focused);
    if (focus.found &&
        !workspace.columns[focus.column].members[focus.member].minimized) {
      target = focusedLeaf(workspace.root.get(),
                            workspace.columns[focus.column].id, true, vertical);
      // Stop recursively crushing the focused window while larger usable
      // slots remain. Keep existing slots and user-adjusted ratios intact.
      if (!splitFits(workspace, target, vertical, gap, minimumWidth, minimumHeight))
        target = nullptr;
    }
  }
  if (!target) {
    qint64 size = -1;
    largestFittingLeaf(workspace, workspace.root.get(), target, vertical, size,
                       gap, minimumWidth, minimumHeight);
  }
  if (!target) {
    // On a crowded or very small output no tile can meet the preferred
    // minimum. Continue the existing bounded layout instead of overlapping
    // windows or silently refusing a new application.
    qint64 size = -1;
    largestLeaf(workspace, workspace.root.get(), target, size);
    if (target)
      vertical = target->geometry.width() >= target->geometry.height();
  }
  if (!workspace.root) {
    workspace.root = std::make_unique<Split>();
    workspace.root->column = id;
  } else {
    // If all windows are minimized, retain their tree for later restoration.
    if (!target)
      target = workspace.root.get();
    const bool firstSplit = target == workspace.root.get() && !target->first;
    auto original = std::make_unique<Split>(std::move(*target));
    // The first pair forms a horizontal row; nested focused splits alternate
    // axes to allow a large side pane with progressively divided neighbors.
    target->vertical = firstSplit || vertical;
    target->ratio = 500000;
    auto added = std::make_unique<Split>();
    added->column = id;
    if (firstSplit && firstWindowOnRight) {
      target->first = std::move(added);
      target->second = std::move(original);
    } else {
      target->first = std::move(original);
      target->second = std::move(added);
    }
    target->column = 0;
  }
  workspace.columns.push_back({0, {member}, id, {}});
}

bool pruneSplit(std::unique_ptr<Split> &node, quint64 id) {
  if (!node)
    return false;
  if (!node->first) {
    if (node->column != id)
      return false;
    node.reset();
    return true;
  }
  const bool removed =
      pruneSplit(node->first, id) || pruneSplit(node->second, id);
  if (!node->first || !node->second) {
    auto remaining =
        node->first ? std::move(node->first) : std::move(node->second);
    node = std::move(remaining);
  }
  return removed;
}

void eraseColumn(Workspace &workspace, std::size_t index) {
  pruneSplit(workspace.root, workspace.columns[index].id);
  workspace.columns.erase(workspace.columns.begin() +
                          static_cast<std::ptrdiff_t>(index));
}

bool splitPath(Split *node, quint64 column, std::vector<Split *> &path) {
  if (!node)
    return false;
  path.push_back(node);
  if ((!node->first && node->column == column) ||
      splitPath(node->first.get(), column, path) ||
      splitPath(node->second.get(), column, path))
    return true;
  path.pop_back();
  return false;
}

bool resizeSplit(Workspace &workspace, quint64 column, int delta, bool vertical,
                 int gap) {
  std::vector<Split *> path;
  if (!splitPath(workspace.root.get(), column, path))
    return false;
  for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
    auto *node = path[i];
    if (node->vertical != vertical)
      continue;
    const auto a = minimumSize(workspace, node->first.get());
    const auto b = minimumSize(workspace, node->second.get());
    if (a.isEmpty() || b.isEmpty())
      continue;
    const int length =
        vertical ? node->geometry.width() : node->geometry.height();
    const int minA = vertical ? a.width() : a.height();
    const int minB = vertical ? b.width() : b.height();
    const int available =
        length - std::min(gap, std::max(0, length - minA - minB));
    if (available < minA + minB)
      return false;
    const int current = vertical ? node->first->geometry.width()
                                 : node->first->geometry.height();
    const int desired = std::clamp(
        current + (path[i + 1] == node->first.get() ? delta : -delta), minA,
        available - minB);
    node->ratio = static_cast<int>((qint64(desired) * 1000000 + available / 2) /
                                   available);
    return true;
  }
  return false;
}

bool focusDirection(Workspace &workspace, int dx, int dy) {
  const auto current = findWindow(workspace, workspace.focused);
  if (!current.found)
    return false;
  const QPoint origin = workspace.columns[current.column]
                            .members[current.member]
                            .geometry.center();
  qint64 best = std::numeric_limits<qint64>::max();
  LayoutWindowId selected = 0;
  for (const auto &column : workspace.columns)
    for (const auto &member : column.members) {
      if (member.minimized || member.window == workspace.focused ||
          !member.geometry.isValid())
        continue;
      const QPoint offset = member.geometry.center() - origin;
      const int forward = offset.x() * dx + offset.y() * dy;
      if (forward <= 0)
        continue;
      const qint64 cross = qAbs(offset.x() * dy - offset.y() * dx);
      const qint64 score = qint64(forward) * forward + cross * cross * 4;
      if (score < best) {
        best = score;
        selected = member.window;
      }
    }
  if (selected)
    workspace.focused = selected;
  return selected != 0;
}
} // namespace

class TilingLayout::Impl {
public:
  Impl(int requestedDefaultWidth, int requestedGap)
      : defaultWidth(std::max(1, requestedDefaultWidth)),
        gap(std::max(0, requestedGap)) {}

  std::pair<LayoutWorkspaceId, Workspace *> locate(LayoutWindowId window) {
    for (auto &[id, workspace] : workspaces) {
      if (findWindow(workspace, window).found)
        return {id, &workspace};
    }
    return {0, nullptr};
  }

  std::pair<LayoutWorkspaceId, const Workspace *>
  locate(LayoutWindowId window) const {
    for (const auto &[id, workspace] : workspaces) {
      if (findWindow(workspace, window).found)
        return {id, &workspace};
    }
    return {0, nullptr};
  }

  int defaultWidth;
  int gap;
  bool splitFocused = true;
  bool firstWindowOnRight = true;
  int minimumTileWidth = 320;
  int minimumTileHeight = 220;
  std::unordered_map<LayoutWorkspaceId, Workspace> workspaces;
};

TilingLayout::TilingLayout() : d(std::make_unique<Impl>(1120, 12)) {}
TilingLayout::~TilingLayout() = default;
TilingLayout::TilingLayout(TilingLayout &&) noexcept = default;
TilingLayout &TilingLayout::operator=(TilingLayout &&) noexcept = default;

void TilingLayout::configure(const QJsonObject &settings) {
  d->defaultWidth =
      std::clamp(settings.value("defaultWidth").toInt(d->defaultWidth), 240,
                 2400);
  d->gap = std::clamp(settings.value("gap").toInt(d->gap), 0, 64);
  d->minimumTileWidth = std::clamp(
      settings.value("minimumTileWidth").toInt(d->minimumTileWidth), 1, 1200);
  d->minimumTileHeight = std::clamp(
      settings.value("minimumTileHeight").toInt(d->minimumTileHeight), 1, 900);
  const auto splitTarget = settings.value("splitTarget").toString();
  if (splitTarget == "focused" || splitTarget == "largest")
    d->splitFocused = splitTarget == "focused";
  const auto firstWindowSide = settings.value("firstWindowSide").toString();
  if (firstWindowSide == "right" || firstWindowSide == "left")
    d->firstWindowOnRight = firstWindowSide == "right";
}

bool TilingLayout::insert(LayoutWorkspaceId workspaceId, LayoutWindowId window,
                          QSize preferredSize) {
  if (!window || d->locate(window).second)
    return false;
  auto &workspace = d->workspaces[workspaceId];
  if (workspace.columns.size() >= kMaximumColumns)
    return false;
  if (workspace.columns.empty())
    workspace.preferredSize =
        !preferredSize.isEmpty()
            ? preferredSize
            : QSize(d->defaultWidth, std::max(1, d->defaultWidth * 2 / 3));
  addColumn(workspace, {window, false, {}}, d->gap, d->splitFocused,
            d->firstWindowOnRight, d->minimumTileWidth, d->minimumTileHeight);
  workspace.focused = window;
  return true;
}

bool TilingLayout::remove(LayoutWindowId window) {
  auto [workspaceId, workspace] = d->locate(window);
  if (!workspace)
    return false;
  if (workspace->maximized == window)
    workspace->maximized = 0;
  const auto location = findWindow(*workspace, window);
  auto &members = workspace->columns[location.column].members;
  members.erase(members.begin() + static_cast<std::ptrdiff_t>(location.member));
  if (members.empty())
    eraseColumn(*workspace, location.column);
  selectAvailableFocus(*workspace);
  if (workspace->columns.empty())
    d->workspaces.erase(workspaceId);
  return true;
}

bool TilingLayout::setMinimized(LayoutWindowId window, bool minimized) {
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

bool TilingLayout::setMaximized(LayoutWindowId window, bool maximized) {
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

bool TilingLayout::moveToWorkspace(LayoutWindowId window,
                                   LayoutWorkspaceId destinationId) {
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
  sourceColumn.members.erase(sourceColumn.members.begin() +
                             static_cast<std::ptrdiff_t>(location.member));
  if (sourceColumn.members.empty())
    eraseColumn(*source, location.column);
  addColumn(destination, member, d->gap, d->splitFocused,
            d->firstWindowOnRight, d->minimumTileWidth, d->minimumTileHeight);
  if (!member.minimized)
    destination.focused = window;
  selectAvailableFocus(*source);
  if (source->columns.empty())
    d->workspaces.erase(sourceId);
  selectAvailableFocus(destination);
  return true;
}

bool TilingLayout::focus(LayoutWindowId window) {
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

bool TilingLayout::focusLeft(LayoutWorkspaceId workspaceId) {
  const auto found = d->workspaces.find(workspaceId);
  return found != d->workspaces.end() && focusDirection(found->second, -1, 0);
}

bool TilingLayout::focusRight(LayoutWorkspaceId workspaceId) {
  const auto found = d->workspaces.find(workspaceId);
  return found != d->workspaces.end() && focusDirection(found->second, 1, 0);
}

bool TilingLayout::focusUp(LayoutWorkspaceId workspaceId) {
  const auto found = d->workspaces.find(workspaceId);
  return found != d->workspaces.end() && focusDirection(found->second, 0, -1);
}

bool TilingLayout::focusDown(LayoutWorkspaceId workspaceId) {
  const auto found = d->workspaces.find(workspaceId);
  return found != d->workspaces.end() && focusDirection(found->second, 0, 1);
}

bool TilingLayout::performAction(const QString &action,
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
  if (action == "group-direction") {
    if (!focus(window))
      return false;
    const int dx = payload.value("dx").toInt();
    const int dy = payload.value("dy").toInt();
    const bool found = dx < 0   ? focusLeft(workspace)
                       : dx > 0 ? focusRight(workspace)
                       : dy < 0 ? focusUp(workspace)
                                : dy > 0 && focusDown(workspace);
    if (!found)
      return false;
    const auto destination = snapshot(workspace).focusedWindow;
    const bool grouped = groupWith(window, destination);
    focus(window);
    return grouped;
  }
  if (action == "group-with")
    return groupWith(window, target);
  if (action == "expel")
    return expel(window);
  if (action == "swap")
    return swapWindows(window, target);
  if (action == "insert-beside")
    return insertBeside(window, target, payload.value("after").toBool());
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

bool TilingLayout::groupWith(LayoutWindowId window, LayoutWindowId target) {
  return insertBeside(window, target, true);
}

bool TilingLayout::swapWindows(LayoutWindowId window, LayoutWindowId target) {
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

bool TilingLayout::insertBeside(LayoutWindowId window, LayoutWindowId target,
                                bool after) {
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
    eraseColumn(*workspace, a.column);
  b = findWindow(*workspace, target);
  auto &destination = workspace->columns[b.column].members;
  destination.insert(destination.begin() + static_cast<std::ptrdiff_t>(
                                               b.member + (after ? 1 : 0)),
                     moving);
  workspace->focused = window;
  return true;
}

bool TilingLayout::moveSingle(LayoutWindowId window, QPoint delta, QRect area) {
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
  const QPoint position = geometry.topLeft() + delta;
  workspace->singleGeometry =
      QRect(QPoint(std::clamp(position.x(), area.left(),
                              area.x() + area.width() - geometry.width()),
                   std::clamp(position.y(), area.top(),
                              area.y() + area.height() - geometry.height())),
            geometry.size());
  return true;
}

bool TilingLayout::resizeHeight(LayoutWindowId window, int height) {
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
    if (allActive != 1)
      return resizeSplit(*workspace, workspace->columns[location.column].id,
                         height - members[location.member].geometry.height(),
                         false, d->gap);
    workspace->singleGeometry.setHeight(
        std::clamp(height, std::min(48, workspace->area.height()),
                   workspace->area.height()));
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

bool TilingLayout::expel(LayoutWindowId window) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace)
    return false;
  const auto location = findWindow(*workspace, window);
  if (workspace->columns[location.column].members.size() == 1 ||
      workspace->columns.size() >= kMaximumColumns)
    return false;
  auto &column = workspace->columns[location.column];
  const Member member = column.members[location.member];
  column.members.erase(column.members.begin() +
                       static_cast<std::ptrdiff_t>(location.member));
  addColumn(*workspace, member, d->gap, d->splitFocused,
            d->firstWindowOnRight, d->minimumTileWidth, d->minimumTileHeight);
  workspace->focused = window;
  return true;
}

bool TilingLayout::reorder(LayoutWindowId window, int direction) {
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace || direction == 0)
    return false;
  workspace->focused = window;
  if (!focusDirection(*workspace, direction < 0 ? -1 : 1, 0))
    return false;
  const auto source = findWindow(*workspace, window);
  const auto target = findWindow(*workspace, workspace->focused);
  std::swap(workspace->columns[source.column].members,
            workspace->columns[target.column].members);
  workspace->focused = window;
  return true;
}

bool TilingLayout::resize(LayoutWindowId window, int width) {
  if (width <= 0)
    return false;
  auto [workspaceId, workspace] = d->locate(window);
  Q_UNUSED(workspaceId);
  if (!workspace)
    return false;
  const auto location = findWindow(*workspace, window);
  auto &column = workspace->columns[location.column];
  int count = 0;
  for (const auto &item : workspace->columns)
    count += static_cast<int>(activeCount(item));
  if (count == 1) {
    workspace->singleGeometry.setWidth(std::clamp(
        width, std::min(48, workspace->area.width()), workspace->area.width()));
    return true;
  }
  return resizeSplit(*workspace, column.id, width - column.geometry.width(),
                     true, d->gap);
}

bool TilingLayout::center(LayoutWindowId window, QRect area) {
  const auto [id, workspace] = d->locate(window);
  Q_UNUSED(id);
  if (!workspace || !workspace->singleGeometry.isValid())
    return false;
  workspace->singleGeometry.moveCenter(area.center());
  return true;
}

QList<WindowPlacement> TilingLayout::layout(LayoutWorkspaceId workspaceId,
                                            QRect area) {
  auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end() || !area.isValid())
    return {};
  auto &workspace = found->second;
  workspace.area = area;
  int count = 0;
  for (auto &column : workspace.columns) {
    count += static_cast<int>(activeCount(column));
    column.geometry = {};
    for (auto &member : column.members)
      member.geometry = {};
  }
  if (count > 1)
    workspace.singleGeometry = {};
  if (!placeSplits(workspace, workspace.root.get(), area, d->gap))
    return {};
  for (auto &column : workspace.columns) {
    std::vector<Member *> active;
    std::vector<int> weights;
    for (auto &member : column.members)
      if (!member.minimized) {
        active.push_back(&member);
        weights.push_back(member.weight);
      }
    if (active.empty())
      continue;
    if (count == 1) {
      auto &geometry = workspace.singleGeometry;
      if (!geometry.isValid()) {
        QSize size = workspace.preferredSize;
        if (size.width() > area.width() || size.height() > area.height())
          size.scale(area.size(), Qt::KeepAspectRatio);
        geometry = QRect(QPoint(), size);
        geometry.moveCenter(area.center());
      }
      geometry.setSize(geometry.size().boundedTo(area.size()));
      geometry.moveLeft(std::clamp(geometry.x(), area.left(),
                                   area.x() + area.width() - geometry.width()));
      geometry.moveTop(
          std::clamp(geometry.y(), area.top(),
                     area.y() + area.height() - geometry.height()));
      active.front()->geometry = geometry;
      column.geometry = geometry;
      column.width = geometry.width();
      continue;
    }
    const auto &rect = column.geometry;
    std::vector<LuDashRectangle> rows(active.size());
    if (ludash_layout_weighted_column_windows(
            {rect.x(), rect.y(), rect.width(), rect.height()}, weights.data(),
            active.size(), d->gap, rows.data(), rows.size()) != rows.size())
      return {};
    for (std::size_t i = 0; i < active.size(); ++i)
      active[i]->geometry =
          QRect(rows[i].x, rows[i].y, rows[i].width, rows[i].height);
  }
  const auto placements = filterPlacements(workspaceId, area, snapshot(workspaceId).columns);
  // Store the effective geometry so focus, drag hit testing and previews agree
  // with the plugin's placement. Tree membership and saved weights stay intact.
  for (auto &column : workspace.columns) {
    column.geometry = {};
    for (auto &member : column.members) {
      for (const auto &placement : placements)
        if (placement.window == member.window)
          member.geometry = placement.geometry;
      if (!member.minimized)
        column.geometry = column.geometry.united(member.geometry);
    }
    column.width = column.geometry.width();
  }
  return snapshot(workspaceId).columns;
}

WorkspaceLayoutSnapshot
TilingLayout::snapshot(LayoutWorkspaceId workspaceId) const {
  WorkspaceLayoutSnapshot result;
  result.workspace = workspaceId;
  const auto found = d->workspaces.find(workspaceId);
  if (found == d->workspaces.end())
    return result;
  const auto &workspace = found->second;
  result.focusedWindow = workspace.focused;
  result.maximizedWindow = workspace.maximized;
  for (std::size_t columnIndex = 0; columnIndex < workspace.columns.size();
       ++columnIndex) {
    const auto &column = workspace.columns[columnIndex];
    QJsonArray members;
    for (const auto &member : column.members)
      members.append(static_cast<qint64>(member.window));
    int rowIndex = 0;
    for (const auto &member : column.members) {
      const int row = member.minimized ? -1 : rowIndex++;
      WindowPlacement placement;
      placement.window = member.window;
      placement.width = column.width;
      placement.minimized = member.minimized;
      placement.focused = member.window == workspace.focused;
      placement.geometry = member.geometry;
      placement.metadata = {
          {"group", static_cast<int>(columnIndex)},
          {"row", row},
          {"members", members}};
      result.columns.append(placement);
    }
  }
  return result;
}

QList<WindowPlacement> TilingLayout::presentation(LayoutWorkspaceId workspaceId,
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

} // namespace LunaDash
