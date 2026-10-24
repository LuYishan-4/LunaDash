#pragma once

#include "compositor/layout/WindowLayout.hpp"
#include <memory>
#include <vector>

namespace LunaDash {

class TilingLayout final : public WindowLayout {
public:
  TilingLayout();
  ~TilingLayout() override;
  TilingLayout(TilingLayout &&) noexcept;
  TilingLayout &operator=(TilingLayout &&) noexcept;
  TilingLayout(const TilingLayout &) = delete;
  TilingLayout &operator=(const TilingLayout &) = delete;

  void configure(const Json &settings) override;
  bool insert(LayoutWorkspaceId workspace, LayoutWindowId window,
              Size preferredSize = {}) override;
  bool remove(LayoutWindowId window) override;
  bool setMinimized(LayoutWindowId window, bool minimized) override;
  bool setMaximized(LayoutWindowId window, bool maximized) override;
  bool moveToWorkspace(LayoutWindowId window,
                       LayoutWorkspaceId workspace) override;
  bool focus(LayoutWindowId window) override;
  bool performAction(const std::string &action,
                     const Json &payload) override;

  std::vector<WindowPlacement> layout(LayoutWorkspaceId workspace,
                                      Rect area) override;
  // Overlay maximization without changing the saved tile sizes or membership.
  std::vector<WindowPlacement> presentation(LayoutWorkspaceId workspace,
                                            Rect area) override;
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
  bool moveSingle(LayoutWindowId window, Point delta, Rect area);
  bool reorder(LayoutWindowId window, int direction);
  bool resize(LayoutWindowId window, int width);
  bool center(LayoutWindowId window, Rect area);

  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
