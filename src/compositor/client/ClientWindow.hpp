#pragma once

#include <QRect>
#include <QSize>
#include <QString>

struct wlr_scene_tree;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;

namespace LunaDash {

struct ClientWindow {
  wlr_xdg_toplevel *toplevel = nullptr;
  wlr_xdg_surface *surface = nullptr;
  wlr_scene_tree *sceneTree = nullptr;
  void *nativeState = nullptr;
  int workspace = 0;
  int id = 0;
  qint64 processId = 0;
  bool floating = false;
  bool desktop = false;
  bool mapped = false;
  bool minimized = false;
  bool maximized = false;
  // True fullscreen occupies the entire output. It is separate from maximized,
  // which remains constrained to the normal panel/gap work area.
  bool fullscreen = false;
  bool hiddenByMaximize = false;
  bool manualResize = false;
  bool initialRuleApplied = false;
  QSize preferredFloatingSize;
  bool utility = false;
  QString appId;
  QString title;
  QString iconName;
  QSize lastSize;
  QRect geometry;
  QRect manualGeometry;
  QRect resizeGuideGeometry;
};

} // namespace LunaDash
