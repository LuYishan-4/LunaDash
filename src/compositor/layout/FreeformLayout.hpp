#pragma once

#include "compositor/layout/WindowLayout.hpp"
#include <QList>
#include <QRect>
#include <QtGlobal>
#include <memory>

namespace LunaDash {

// Generic persistent geometry state for replacement window-layout plugins that
// request layoutMode=stacking. Placement policy (cascade, grid, etc.) belongs
// to the plugin; this class only keeps focus, size, position and workspace state.
class FreeformLayout final : public WindowLayout {
public:
  FreeformLayout();
  ~FreeformLayout() override;
  FreeformLayout(const FreeformLayout &) = delete;
  FreeformLayout &operator=(const FreeformLayout &) = delete;

  void configure(const QJsonObject &settings) override;
  bool insert(LayoutWorkspaceId workspace, LayoutWindowId window,
              QSize preferredSize = {}) override;
  bool remove(LayoutWindowId window) override;
  bool setMinimized(LayoutWindowId window, bool minimized) override;
  bool setMaximized(LayoutWindowId window, bool maximized) override;
  bool moveToWorkspace(LayoutWindowId window,
                       LayoutWorkspaceId workspace) override;
  bool focus(LayoutWindowId window) override;
  bool performAction(const QString &action,
                     const QJsonObject &payload) override;

  QList<WindowPlacement> layout(LayoutWorkspaceId workspace,
                                QRect area) override;
  // Overlay maximization without changing the saved tile sizes or membership.
  QList<WindowPlacement> presentation(LayoutWorkspaceId workspace,
                                      QRect area) override;
  WorkspaceLayoutSnapshot snapshot(LayoutWorkspaceId workspace) const override;

private:
  bool focusLeft(LayoutWorkspaceId workspace);
  bool focusRight(LayoutWorkspaceId workspace);
  bool focusUp(LayoutWorkspaceId workspace);
  bool focusDown(LayoutWorkspaceId workspace);
  bool groupWith(LayoutWindowId window, LayoutWindowId targetWindow);
  bool expel(LayoutWindowId window);
  bool swapWindows(LayoutWindowId window, LayoutWindowId target);
  bool insertBeside(LayoutWindowId window, LayoutWindowId target, bool after);
  bool resizeHeight(LayoutWindowId window, int height);
  bool moveSingle(LayoutWindowId window, QPoint delta, QRect area);
  bool reorder(LayoutWindowId window, int direction);
  bool resize(LayoutWindowId window, int width);
  bool center(LayoutWindowId window, QRect area);

  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
