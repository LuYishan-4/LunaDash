#pragma once

#include "compositor/window/animation/SceneAnimation.hpp"
#include "core/geometry/Geometry.hpp"

namespace LunaDash::WindowTuning {


inline constexpr int workspaceSlideGap = 48;

inline constexpr Size defaultFloatingSize{720, 500};

inline constexpr int minimizeDropMinimum = 48;
inline constexpr int minimizeDropDivisor = 6;
// The outline stacking draws around each window.
inline constexpr int stackingBorderWidth = 2;
// Stacking windows: the invisible border outside a window that resizes it,
// how far a corner reaches along its edges, how close an edge comes to the
// screen or another window before it snaps there, and the smallest size a
// resize leaves.
inline constexpr int resizeBorder = 10;
inline constexpr int resizeCorner = 24;
inline constexpr int snapDistance = 12;
inline constexpr Size minimumWindowSize{120, 80};
// A dragged stacking window held this close to the screen's edge quick-tiles
// there; a tiled or maximized one goes back to its own size once dragged this far.
inline constexpr int quickTileEdge = 3;
inline constexpr double restoreDragDistance = 6.0;
// Switching between tiling and stacking, windows travel to their new places
// on this softer spring (about half a second), so the move can be followed.
inline constexpr SceneSpring styleSwitchSpring{110.0, 1.0, 0.5, 10.0};
// Widen/narrow column shortcuts.
inline constexpr int columnResizeStep = 80;
inline constexpr int columnMinimumWidth = 120;

// The workspace overview: workspaces shrink to this scale, stacked with this gap
// (in screen pixels before scaling), zooming on this spring (in units of the zoom).
inline constexpr double overviewZoom = 0.5;
inline constexpr int overviewWorkspaceGap = 96;
inline constexpr SceneSpring overviewSpring{800.0, 0.92, 0.001, 0.01};
// Sliding between workspaces in the overview (logical pixels).
inline constexpr SceneSpring overviewViewSpring{500.0, 1.0, 0.5, 5.0};
// The session splash fading out once the desktop is up (in units of its alpha).
inline constexpr SceneSpring splashFadeSpring{120.0, 1.0, 0.002, 0.02};
// A press that moves further than this drags the window instead of clicking it.
inline constexpr double overviewDragThreshold = 6.0;
// Scroll distance (wheel units) per workspace step.
inline constexpr double overviewScrollStep = 15.0;

} // namespace LunaDash::WindowTuning
