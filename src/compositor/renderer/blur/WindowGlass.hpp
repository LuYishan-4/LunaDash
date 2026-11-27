#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct wl_event_loop;
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

    float opacity = -1.0f;
    bool fading = false;

    bool shared = false;
  };

  // An output's box in scene coordinates.
  struct Output {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
  };

  // `loop` releases explicitly synchronized buffers once their blur is done
  // and times deferred captures; `windowLayer` is where shared backdrops stop.
  WindowGlass(wlr_renderer *renderer, wlr_allocator *allocator,
              wlr_scene_node *root, wlr_scene_node *windowLayer,
              wl_event_loop *loop);
  ~WindowGlass();
  WindowGlass(const WindowGlass &) = delete;
  WindowGlass &operator=(const WindowGlass &) = delete;

  bool configure(bool blurEnabled, int radius, float opacity);
  void update(const std::vector<Surface> &surfaces,
              const std::vector<Output> &outputs, bool animationsActive);
  // Called when a capture put off by throttling is due: schedule a frame.
  void setFrameRequest(std::function<void()> request);
  bool ready() const;
  bool failed() const;
  std::uint64_t frames() const;
  std::string error() const;
  // What the last shared backdrop capture redrew for (layer namespaces).
  std::string cause() const;

private:
  class Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
