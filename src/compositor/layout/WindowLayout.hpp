#pragma once
#include "core/geometry/Geometry.hpp"
#include "core/json/JsonAccess.hpp"

#include <cstdint>
#include <string>
#include <functional>
#include <vector>

namespace LunaDash {

using LayoutWindowId = std::uint64_t;
using LayoutWorkspaceId = std::uint64_t;

struct WindowPlacement {
  LayoutWindowId window = 0;
  int width = 0;
  bool minimized = false;
  bool focused = false;
  Rect geometry;
  Json metadata = Json::object();
  bool hiddenByMaximize = false;
  bool newWindow = false;
};

struct WorkspaceLayoutSnapshot {
  LayoutWorkspaceId workspace = 0;
  LayoutWindowId focusedWindow = 0;
  std::vector<WindowPlacement> columns;
  LayoutWindowId maximizedWindow = 0;
};

// Generic window-layout state contract. Layout type, editable settings and
// interaction policy belong to WindowTemplate, not this interface.
class WindowLayout {
public:
  virtual ~WindowLayout();
  using PlacementFilter = std::function<std::vector<WindowPlacement>(
      LayoutWorkspaceId, Rect, const std::vector<WindowPlacement> &)>;
  void setPlacementFilter(PlacementFilter filter);

  virtual void configure(const Json &settings) = 0;
  virtual bool insert(LayoutWorkspaceId workspace, LayoutWindowId window,
                      Size preferredSize = {}) = 0;
  virtual bool remove(LayoutWindowId window) = 0;
  virtual bool setMinimized(LayoutWindowId window, bool minimized) = 0;
  virtual bool setMaximized(LayoutWindowId window, bool maximized) = 0;
  virtual bool moveToWorkspace(LayoutWindowId window,
                               LayoutWorkspaceId workspace) = 0;
  virtual bool focus(LayoutWindowId window) = 0;

  // Strategy-specific interaction is dispatched by action ID. The generic
  // layout contract intentionally does not encode tiling concepts such as
  // directional focus, groups, columns or resize directions.
  virtual bool performAction(const std::string &action,
                             const Json &payload) = 0;

  virtual std::vector<WindowPlacement> layout(LayoutWorkspaceId workspace,
                                              Rect area) = 0;
  virtual std::vector<WindowPlacement> presentation(
      LayoutWorkspaceId workspace, Rect area) = 0;
  virtual WorkspaceLayoutSnapshot
  snapshot(LayoutWorkspaceId workspace) const = 0;

protected:
  std::vector<WindowPlacement> filterPlacements(
      LayoutWorkspaceId workspace, Rect area,
      const std::vector<WindowPlacement> &placements) const;

private:
  PlacementFilter placementFilter_;
};

} // namespace LunaDash
