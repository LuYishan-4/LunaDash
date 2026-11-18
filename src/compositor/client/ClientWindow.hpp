#pragma once

#include "core/geometry/Geometry.hpp"

#include <cstdint>
#include <string>

struct wlr_scene_tree;
struct wlr_surface;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;
struct wlr_xwayland_surface;

namespace LunaDash {

struct ClientWindow {
  wlr_xdg_toplevel *toplevel = nullptr;
  wlr_xdg_surface *surface = nullptr;
  wlr_xwayland_surface *xwayland = nullptr;
  wlr_surface *wlSurface = nullptr;
  wlr_scene_tree *sceneTree = nullptr;
  wlr_scene_tree *sceneContent = nullptr;
  void *nativeState = nullptr;
  int workspace = 0;
  int id = 0;
  std::int64_t processId = 0;
  bool floating = false;
  bool x11 = false;
  bool desktop = false;
  bool mapped = false;
  // Reserve a tiled slot after the initial commit, before the first buffer.
  // Visibility and input focus still require mapped to be true.
  bool layoutPending = false;
  bool minimized = false;
  bool maximized = false;
  // True fullscreen occupies the entire output. It is separate from maximized,
  // which remains constrained to the normal panel/gap work area.
  bool fullscreen = false;
  bool hiddenByMaximize = false;
  bool manualResize = false;
  // Stacking windows zoom instead of resizing: the client keeps laying itself
  // out at its own size and is drawn this many times as large
  // (clientContentScale, applyClientContentScale).
  double contentScale = 1.0;
  // The size a resize animation draws the window at right now (empty when
  // none runs); its live buffers are stretched to it (applyClientContentScale).
  Size animatedSize;
  // The scale the scene currently draws the window at, per axis.
  double appliedScaleX = 1.0;
  double appliedScaleY = 1.0;
  bool initialRuleApplied = false;
  Size preferredFloatingSize;
  bool utility = false;
  std::string appId;
  std::string title;
  std::string iconName;
  Size lastSize;
  Rect geometry;
  Rect manualGeometry;
  Rect resizeGuideGeometry;
};

} // namespace LunaDash
