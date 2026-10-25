#pragma once

#include "compositor/layout/WindowLayout.hpp"

#include <memory>

namespace LunaDash {

// Scrollable columns in the spirit of niri: every window keeps its column
// width, new windows open as a column to the right of the focused one, and the
// view scrolls to keep the focused column on screen. A column can stack
// several windows vertically. Nothing is stretched to fill the screen; a strip
// narrower than the screen is centred.
class ScrollingLayout final : public WindowLayout {
public:
  ScrollingLayout();
  ~ScrollingLayout() override;
  ScrollingLayout(const ScrollingLayout &) = delete;
  ScrollingLayout &operator=(const ScrollingLayout &) = delete;

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
  std::vector<WindowPlacement> presentation(LayoutWorkspaceId workspace,
                                            Rect area) override;
  WorkspaceLayoutSnapshot snapshot(LayoutWorkspaceId workspace) const override;

private:
  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
