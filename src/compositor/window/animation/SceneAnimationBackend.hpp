#pragma once

#include <QEasingCurve>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QRect>

struct wlr_scene_tree;

namespace LunaDash {

// wlroots scene implementation for the generic window-animation template.
// Policy is supplied by WindowTemplate/plugins; this class only executes it.
class SceneAnimationBackend final {
public:
  SceneAnimationBackend() = default;
  ~SceneAnimationBackend();

  void configure(const QJsonObject &profile);
  void open(wlr_scene_tree *tree, const QRect &geometry);
  void close(wlr_scene_tree *source, wlr_scene_tree *parent);
  void relayout(wlr_scene_tree *tree, const QRect &previous,
                const QRect &geometry, wlr_scene_tree *overlay,
                bool animate = true);
  void focus(wlr_scene_tree *tree, const QRect &geometry);
  void cancel(wlr_scene_tree *tree);
  void clear();
  int activeCount() const;
  void advance();
  QRect visualGeometry(wlr_scene_tree *tree, const QRect &fallback) const;

private:
  struct LiveState;
  struct SnapshotState;

  void setDuration(int milliseconds);
  void applyLive(LiveState *state, qreal progress);
  void finishLive(wlr_scene_tree *tree, LiveState *state, bool restore = true);
  void startLive(LiveState *state);
  SnapshotState *createSnapshot(wlr_scene_tree *source, wlr_scene_tree *parent,
                                const QRect &geometry);
  void destroySnapshot(SnapshotState *state);
  void applySnapshot(SnapshotState *state, const QRect &geometry,
                     qreal opacity);

  QHash<wlr_scene_tree *, LiveState *> live_;
  QList<SnapshotState *> snapshots_;
  int duration_ = 220;
  int enterOffset_ = 12;
  qreal focusOpacity_ = 0.82;
  qreal exitScale_ = 0.90;
  QEasingCurve easing_{QEasingCurve::OutCubic};
};

} // namespace LunaDash
