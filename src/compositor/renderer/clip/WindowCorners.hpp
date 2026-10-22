#pragma once

#include <QList>
#include <QPoint>
#include <QSize>
#include <memory>

struct wlr_scene_tree;
struct wlr_surface;

namespace LunaDash {

// Rounded visibility through standard clipped scene-surface views. Clients
// retain wlroots' regular buffer synchronization and input-coordinate handling.
class WindowCorners final {
public:
  struct Surface {
    wlr_scene_tree *tree = nullptr;
    wlr_scene_tree *content = nullptr;
    wlr_surface *surface = nullptr;
    QSize size;
    QPoint surfaceOrigin;
    bool enabled = true;
    float opacity = 1.0f;
  };

  WindowCorners();
  ~WindowCorners();
  WindowCorners(const WindowCorners &) = delete;
  WindowCorners &operator=(const WindowCorners &) = delete;

  void update(const QList<Surface> &surfaces);
  int radius(wlr_scene_tree *tree) const;

private:
  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
