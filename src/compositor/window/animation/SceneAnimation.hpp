#pragma once

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

struct SceneAnimationOptions {
  int duration = 220;
  int enterOffset = 12;
  double focusOpacity = 0.82;
  double exitScale = 0.90;
  SceneAnimationCurve curve = SceneAnimationCurve::OutCubic;
};

// wlroots scene implementation for the generic window-animation template.
// Policy is supplied by WindowTemplate/plugins; this class only executes it.
class SceneAnimation final {
public:
  SceneAnimation() = default;
  ~SceneAnimation();

  void configure(const SceneAnimationOptions &options);
  void open(wlr_scene_tree *tree, const SceneAnimationRectangle &geometry);
  // Fades in while moving from geometry offset by (dx, dy) to geometry.
  void open(wlr_scene_tree *tree, const SceneAnimationRectangle &geometry, int dx,
            int dy);
  void close(wlr_scene_tree *source, wlr_scene_tree *parent);
  // Fades a snapshot out while it slides by (dx, dy) instead of shrinking.
  void close(wlr_scene_tree *source, wlr_scene_tree *parent, int dx, int dy);
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
  void advance();
  SceneAnimationRectangle
  visualGeometry(wlr_scene_tree *tree,
                 const SceneAnimationRectangle &fallback) const;

private:
  struct LiveState;
  struct SnapshotState;

  void setDuration(int milliseconds);
  void applyLive(LiveState *state, double progress);
  void finishLive(wlr_scene_tree *tree, LiveState *state, bool restore = true);
  void startLive(LiveState *state);
  SnapshotState *createSnapshot(wlr_scene_tree *source, wlr_scene_tree *parent,
                                const SceneAnimationRectangle &geometry);
  void destroySnapshot(SnapshotState *state);
  void applySnapshot(SnapshotState *state,
                     const SceneAnimationRectangle &geometry, double opacity);

  std::unordered_map<wlr_scene_tree *, LiveState *> live_;
  std::vector<SnapshotState *> snapshots_;
  int duration_ = 220;
  int enterOffset_ = 12;
  double focusOpacity_ = 0.82;
  double exitScale_ = 0.90;
  SceneAnimationCurve curve_ = SceneAnimationCurve::OutCubic;
};

} // namespace LunaDash
