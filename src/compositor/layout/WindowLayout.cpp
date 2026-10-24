#include "compositor/layout/WindowLayout.hpp"
namespace LunaDash {
WindowLayout::~WindowLayout() = default;
void WindowLayout::setPlacementFilter(PlacementFilter filter) {
  placementFilter_ = std::move(filter);
}
std::vector<WindowPlacement>
WindowLayout::filterPlacements(LayoutWorkspaceId workspace, Rect area,
                               const std::vector<WindowPlacement> &placements) const {
  return placementFilter_ ? placementFilter_(workspace, area, placements) : placements;
}
} // namespace LunaDash
