#pragma once

#include "core/geometry/Geometry.hpp"

namespace LunaDash::WindowTuning {

// Extra distance between the outgoing and incoming workspace during a slide.
inline constexpr int workspaceSlideGap = 48;
// Floating windows that never announced a size open at this size.
inline constexpr Size defaultFloatingSize{720, 500};
// Minimizing drops a window toward the dock by a sixth of its height, at least this far.
inline constexpr int minimizeDropMinimum = 48;
inline constexpr int minimizeDropDivisor = 6;
// Widen/narrow column shortcuts.
inline constexpr int columnResizeStep = 80;
inline constexpr int columnMinimumWidth = 120;

} // namespace LunaDash::WindowTuning
