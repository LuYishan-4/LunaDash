#pragma once

#include <functional>
#include <memory>
#include <vector>

struct wl_display;
struct wlr_output;
struct wlr_scene_tree;
struct wlr_surface;

namespace LunaDash {

class SessionLock final {
public:
  struct Output {
    wlr_output *output = nullptr;
    int x = 0, y = 0, width = 0, height = 0;  // in the output layout
  };
  struct Callbacks {
    // Gives `surface` (a lock surface, or nullptr for none) keyboard focus.
    std::function<void(wlr_surface *)> focus;
    // The session became locked, or unlocked.
    std::function<void(bool locked)> changed;
  };

  SessionLock(wl_display *display, wlr_scene_tree *root, Callbacks callbacks);
  ~SessionLock();
  SessionLock(const SessionLock &) = delete;
  SessionLock &operator=(const SessionLock &) = delete;

  bool valid() const;
  // Locked, including a locker that died without unlocking.
  bool locked() const { return locked_; }
  bool isLockSurface(wlr_surface *surface) const;
  // The lock surface under layout coordinates (lx, ly), or nullptr.
  wlr_surface *surfaceAt(double lx, double ly, double *sx, double *sy) const;
  // Outputs and their layout boxes; lock surfaces and the blank fill follow them.
  void setOutputs(const std::vector<Output> &outputs);

  struct State;

private:
  bool locked_ = false;
  std::unique_ptr<State> d;
};

} // namespace LunaDash
