#pragma once

#include <QList>
#include <QRect>
#include <QtGlobal>
#include <memory>

namespace LunaDash {

using TilingWindowId = quint64;
using TilingWorkspaceId = quint64;

struct TilingColumnSnapshot {
  TilingWindowId window = 0;
  int width = 0;
  bool minimized = false;
  bool focused = false;
  QRect geometry;
  int columnIndex = -1;
  int rowIndex = -1;
  QList<TilingWindowId> columnMembers;
};

struct TilingWorkspaceSnapshot {
  TilingWorkspaceId workspace = 0;
  int scrollOffset = 0;
  TilingWindowId focusedWindow = 0;
  QList<TilingColumnSnapshot> columns;
};

class ScrollableTilingLayout {
public:
  explicit ScrollableTilingLayout(int defaultWidth = 720, int gap = 12);
  ~ScrollableTilingLayout();
  ScrollableTilingLayout(ScrollableTilingLayout &&) noexcept;
  ScrollableTilingLayout &operator=(ScrollableTilingLayout &&) noexcept;
  ScrollableTilingLayout(const ScrollableTilingLayout &) = delete;
  ScrollableTilingLayout &operator=(const ScrollableTilingLayout &) = delete;

  void setGap(int gap);
  bool insert(TilingWorkspaceId workspace, TilingWindowId window,
              int width = 0);
  bool remove(TilingWindowId window);
  bool setMinimized(TilingWindowId window, bool minimized);
  bool moveToWorkspace(TilingWindowId window, TilingWorkspaceId workspace);
  bool focus(TilingWindowId window);
  bool focusLeft(TilingWorkspaceId workspace);
  bool focusRight(TilingWorkspaceId workspace);
  bool focusUp(TilingWorkspaceId workspace);
  bool focusDown(TilingWorkspaceId workspace);
  bool groupWith(TilingWindowId window, TilingWindowId targetWindow);
  bool expel(TilingWindowId window);
  bool swapWindows(TilingWindowId window, TilingWindowId target);
  bool insertBeside(TilingWindowId window, TilingWindowId target, bool after);
  bool resizeHeight(TilingWindowId window, int height);
  bool moveSingle(TilingWindowId window, QPoint delta, QRect area);
  bool reorder(TilingWindowId window, int direction);
  bool resize(TilingWindowId window, int width);
  bool center(TilingWindowId window, QRect area);

  QList<TilingColumnSnapshot> layout(TilingWorkspaceId workspace, QRect area);
  TilingWorkspaceSnapshot snapshot(TilingWorkspaceId workspace) const;

private:
  class Impl;
  std::unique_ptr<Impl> d;
};

QList<QRect> tileRectangles(QRect area, int count, double columnRatio = 0.56,
                            int gap = 12);

} // namespace LunaDash
