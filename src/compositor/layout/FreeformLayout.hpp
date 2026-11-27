#pragma once

#include "compositor/layout/WindowLayout.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace LunaDash {

// The part of `area` a window quick-tiled to `edges` fills (xdg edges: top 1,
// bottom 2, left 4, right 8): a half, or a quarter in a corner, with a gap
// where tiles meet.
Rect quickTileRect(Rect area, std::uint32_t edges);

// A grid of cells over `area`, `gap` apart, one for each of `windows` in
// their reading order (rows from the top, left to right in each), the last
// row centred: the cell of each window, in the order given.
std::vector<Rect> windowGrid(const std::vector<Rect> &windows, Rect area,
                             int gap);

// The stacking template: overlapping windows that keep their own size and
// position, or fill a half or a quarter of the screen (quick tiling). New
// windows open where they cover the others least; window-layout plugins can
// place them otherwise.
class FreeformLayout final : public WindowLayout {
public:
  FreeformLayout();
  ~FreeformLayout() override;
  FreeformLayout(const FreeformLayout &) = delete;
  FreeformLayout &operator=(const FreeformLayout &) = delete;

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
  bool place(LayoutWindowId window, Rect geometry);
  bool tile(LayoutWindowId window, std::uint32_t edges);

  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
