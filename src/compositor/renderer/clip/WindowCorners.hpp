#pragma once

#include <memory>
#include <vector>

struct wlr_scene_tree;
struct wlr_surface;

namespace LunaDash {

// Rounded visibility through standard clipped scene-surface views. Clients
// retain wlroots' regular buffer synchronization and input-coordinate handling.
class WindowCorners final {
public:
  struct Size {
    int width = 0;
    int height = 0;
    bool empty() const { return width <= 0 || height <= 0; }
    bool operator==(const Size &) const = default;
  };

  struct Point {
    int x = 0;
    int y = 0;
    bool operator==(const Point &) const = default;
  };

  struct Surface {
    wlr_scene_tree *tree = nullptr;
    wlr_scene_tree *content = nullptr;
    wlr_surface *surface = nullptr;
    Size size;
    Point surfaceOrigin;
    bool enabled = true;
    float opacity = 1.0f;
  };

  WindowCorners();
  ~WindowCorners();
  WindowCorners(const WindowCorners &) = delete;
  WindowCorners &operator=(const WindowCorners &) = delete;

  void update(const std::vector<Surface> &surfaces);
  int radius(wlr_scene_tree *tree) const;

private:
  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
