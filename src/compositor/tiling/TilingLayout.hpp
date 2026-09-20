#pragma once

#include "compositor/layout/WindowLayout.hpp"
#include <QList>
#include <QRect>
#include <QtGlobal>
#include <memory>

namespace LunaDash {

class TilingLayout final : public WindowLayout {
public:
  explicit TilingLayout(int defaultWidth = 960, int gap = 12);
  ~TilingLayout() override;
  WindowLayoutMode mode() const noexcept override;
  TilingLayout(TilingLayout &&) noexcept;
  TilingLayout &operator=(TilingLayout &&) noexcept;
  TilingLayout(const TilingLayout &) = delete;
  TilingLayout &operator=(const TilingLayout &) = delete;

  void setGap(int gap) override;
  bool insert(LayoutWorkspaceId workspace, LayoutWindowId window,
              QSize preferredSize = {}) override;
  bool remove(LayoutWindowId window) override;
  bool setMinimized(LayoutWindowId window, bool minimized) override;
  bool setMaximized(LayoutWindowId window, bool maximized) override;
  bool moveToWorkspace(LayoutWindowId window,
                       LayoutWorkspaceId workspace) override;
  bool focus(LayoutWindowId window) override;
  bool focusLeft(LayoutWorkspaceId workspace) override;
  bool focusRight(LayoutWorkspaceId workspace) override;
  bool focusUp(LayoutWorkspaceId workspace) override;
  bool focusDown(LayoutWorkspaceId workspace) override;
  bool groupWith(LayoutWindowId window, LayoutWindowId targetWindow) override;
  bool expel(LayoutWindowId window) override;
  bool swapWindows(LayoutWindowId window, LayoutWindowId target) override;
  bool insertBeside(LayoutWindowId window, LayoutWindowId target,
                    bool after) override;
  bool resizeHeight(LayoutWindowId window, int height) override;
  bool moveSingle(LayoutWindowId window, QPoint delta, QRect area) override;
  bool reorder(LayoutWindowId window, int direction) override;
  bool resize(LayoutWindowId window, int width) override;
  bool center(LayoutWindowId window, QRect area) override;

  QList<WindowPlacement> layout(LayoutWorkspaceId workspace,
                                QRect area) override;
  // Overlay maximization without changing the saved tile sizes or membership.
  QList<WindowPlacement> presentation(LayoutWorkspaceId workspace,
                                      QRect area) override;
  WorkspaceLayoutSnapshot snapshot(LayoutWorkspaceId workspace) const override;

private:
  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
