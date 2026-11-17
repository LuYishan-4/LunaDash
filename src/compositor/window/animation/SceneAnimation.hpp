#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

struct wlr_scene_tree;

namespace LunaDash {

struct SceneAnimationRectangle {
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;

  bool valid() const { return width > 0 && height > 0; }
  bool sameSize(const SceneAnimationRectangle &other) const {
    return width == other.width && height == other.height;
  }
  SceneAnimationRectangle translated(int dx, int dy) const {
    return {left + dx, top + dy, width, height};
  }
  friend bool operator==(const SceneAnimationRectangle &,
                         const SceneAnimationRectangle &) = default;
};

enum class SceneAnimationCurve { Linear, OutCubic, OutQuint, InOutCubic };

// A damped spring (unit mass). Interrupted motions keep their velocity.
struct SceneSpring {
  double stiffness = 800.0;
  // 1 is critically damped; below 1 overshoots.
  double dampingRatio = 1.0;
  // Settled once the remaining distance (logical pixels) and the speed
  // (pixels per second) are below these.
  double restDistance = 0.5;
  double restSpeed = 10.0;
};

// Advances a spring by `seconds`: `offset` is the value minus its target. Exact for
// any step length, so the motion does not depend on the frame rate.
void stepSceneSpring(double &offset, double &velocity, const SceneSpring &spring,
                     double seconds);

struct SceneAnimationOptions {
  // Fades and the close animation; 0 disables all animation.
  int duration = 220;
  int closeDuration = 150;
  int enterOffset = 12;
  double focusOpacity = 1.0;
  double exitScale = 0.90;
  // A new window grows from this scale.
  double openScale = 0.92;
  SceneAnimationCurve curve = SceneAnimationCurve::OutCubic;
  SceneSpring windowOpen;
  SceneSpring windowMovement;
  SceneSpring windowResize;
  SceneSpring workspaceSwitch{1000.0, 1.0};
};

// wlroots scene implementation for the generic window-animation template.
// Policy is supplied by WindowTemplate/plugins; this class only executes it.
// Geometry moves on springs; opacity follows `duration` and `curve`.
class SceneAnimation final {
public:
  SceneAnimation() = default;
  ~SceneAnimation();

  void configure(const SceneAnimationOptions &options);
  // Snapshots of opening windows are drawn in this tree.
  void setSnapshotLayer(wlr_scene_tree *layer) { snapshotLayer_ = layer; }
  // A new window: grows from openScale while it fades in, or moves in by
  // enterOffset when no snapshot of it can be taken.
  void open(wlr_scene_tree *tree, const SceneAnimationRectangle &geometry);
  // Fades in while moving from geometry offset by (dx, dy) to geometry; with
  // `fade` false it only moves (workspace slides).
  void open(wlr_scene_tree *tree, const SceneAnimationRectangle &geometry, int dx,
            int dy, bool fade = true);
  void close(wlr_scene_tree *source, wlr_scene_tree *parent);
  // Fades a snapshot out while it slides by (dx, dy) instead of shrinking;
  // with `fade` false it stays opaque.
  void close(wlr_scene_tree *source, wlr_scene_tree *parent, int dx, int dy,
             bool fade = true);
  // A workspace switch: moves the window itself from geometry by (dx, dy) and
  // hides it (disables its node) once it settles. Unlike close() it takes no
  // snapshot, so clients using explicit synchronization slide too. Returns false
  // when nothing animates; the caller then hides the window itself.
  bool slideOut(wlr_scene_tree *tree, const SceneAnimationRectangle &geometry,
                int dx, int dy);
  // True while slideOut() is moving `tree` away.
  bool slidingOut(wlr_scene_tree *tree) const;
  // Turns a slide-out around: the window springs back to where it was.
  void returnHome(wlr_scene_tree *tree);
  // Moves the destination of a running animation (a layer surface that was laid out again).
  void retarget(wlr_scene_tree *tree, const SceneAnimationRectangle &geometry);
  int duration() const { return duration_; }
  void relayout(wlr_scene_tree *tree, const SceneAnimationRectangle &previous,
                const SceneAnimationRectangle &geometry,
                wlr_scene_tree *overlay, bool animate = true);
  void focus(wlr_scene_tree *tree, const SceneAnimationRectangle &geometry);
  void cancel(wlr_scene_tree *tree);
  void clear();
  int activeCount() const;
  // Steps every animation to `presentNs` (CLOCK_MONOTONIC), the time the
  // next frame is expected on screen.
  void advance(std::uint64_t presentNs);
  void advance();
  SceneAnimationRectangle
  visualGeometry(wlr_scene_tree *tree,
                 const SceneAnimationRectangle &fallback) const;

  // Spring motion of a rectangle toward a target, one spring per edge value.
  struct Motion {
    SceneSpring spring;
    SceneAnimationRectangle target;
    // Current value minus target, per left/top/width/height.
    std::array<double, 4> offset{};
    std::array<double, 4> velocity{};
    double initialDistance = 0.0;

    void start(const SceneSpring &parameters, const SceneAnimationRectangle &from,
               const SceneAnimationRectangle &to);
    // A new target; the current position and velocity carry over.
    void retarget(const SceneAnimationRectangle &to);
    void step(double seconds);
    bool settled() const;
    // 0 at the start, 1 at the target (approximately, with overshoot).
    double progress() const;
    SceneAnimationRectangle current() const;
  };

private:
  struct LiveState;
  struct SnapshotState;

  void setDuration(int milliseconds);
  void applyLive(LiveState *state, std::uint64_t nowNs);
  void finishLive(wlr_scene_tree *tree, LiveState *state, bool restore = true);
  void startLive(LiveState *state);
  bool openWithSnapshot(wlr_scene_tree *tree, const SceneAnimationRectangle &geometry);
  SnapshotState *createSnapshot(wlr_scene_tree *source, wlr_scene_tree *parent,
                                const SceneAnimationRectangle &geometry);
  void destroySnapshot(SnapshotState *state);
  void applySnapshot(SnapshotState *state,
                     const SceneAnimationRectangle &geometry, double opacity);

  std::unordered_map<wlr_scene_tree *, LiveState *> live_;
  std::vector<SnapshotState *> snapshots_;
  wlr_scene_tree *snapshotLayer_ = nullptr;
  int duration_ = 220;
  int closeDuration_ = 150;
  int enterOffset_ = 12;
  double focusOpacity_ = 1.0;
  double exitScale_ = 0.90;
  double openScale_ = 0.92;
  SceneAnimationCurve curve_ = SceneAnimationCurve::OutCubic;
  SceneSpring windowOpen_;
  SceneSpring windowMovement_;
  SceneSpring windowResize_;
  SceneSpring workspaceSwitch_{1000.0, 1.0};
};

} // namespace LunaDash
