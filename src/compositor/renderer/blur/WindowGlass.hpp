#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct wlr_allocator;
struct wlr_renderer;
struct wlr_scene_node;
struct wlr_scene_tree;

namespace LunaDash {

// Per-window underlays in the existing wlroots scene. The compositor retains
// window membership, stacking and animation ownership.
class WindowGlass final {
public:
  struct Size {
    int width = 0;
    int height = 0;

    bool valid() const { return width > 0 && height > 0; }
    bool operator==(const Size &) const = default;
  };

  struct Surface {
    wlr_scene_tree *tree = nullptr;
    Size size;
    bool enabled = true;
    int cornerRadius = 16;
    // A shell surface owns its alpha in its client buffer. Negative uses the
    // application-window preference; explicit values never dim shell text.
    float opacity = -1.0f;
  };

  WindowGlass(wlr_renderer *renderer, wlr_allocator *allocator,
              wlr_scene_node *root);
  ~WindowGlass();
  WindowGlass(const WindowGlass &) = delete;
  WindowGlass &operator=(const WindowGlass &) = delete;

  bool configure(bool blurEnabled, int radius, float opacity);
  void update(const std::vector<Surface> &surfaces, bool animationsActive);
  bool ready() const;
  bool failed() const;
  std::uint64_t frames() const;
  std::string error() const;

private:
  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
