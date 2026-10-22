#include "compositor/layout/WindowLayout.hpp"
namespace LunaDash {
WindowLayout::~WindowLayout() = default;
void WindowLayout::setPlacementFilter(PlacementFilter filter) {
  placementFilter_ = std::move(filter);
}
QList<WindowPlacement> WindowLayout::filterPlacements(LayoutWorkspaceId workspace,
    QRect area, const QList<WindowPlacement> &placements) const {
  return placementFilter_ ? placementFilter_(workspace, area, placements) : placements;
}
} // namespace LunaDash
