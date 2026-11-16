#pragma once

#include "compositor/window/animation/SceneAnimation.hpp"
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

// The workspace overview: workspaces shrink to this scale, stacked with this gap
// (in screen pixels before scaling), zooming on this spring (in units of the zoom).
inline constexpr double overviewZoom = 0.5;
inline constexpr int overviewWorkspaceGap = 96;
inline constexpr SceneSpring overviewSpring{800.0, 0.92, 0.001, 0.01};
// A press that moves further than this drags the window instead of clicking it.
inline constexpr double overviewDragThreshold = 6.0;
// Scroll distance (wheel units) per workspace step.
inline constexpr double overviewScrollStep = 15.0;

} // namespace LunaDash::WindowTuning
