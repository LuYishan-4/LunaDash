#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QRect>
#include <QJsonObject>
#include <QEasingCurve>

struct wlr_scene_tree;

namespace LunaDash {

class SceneWindowAnimations final : public QObject {
public:
  explicit SceneWindowAnimations(QObject *parent = nullptr);
  ~SceneWindowAnimations() override;

  void setDuration(int milliseconds);
  void setProfile(const QJsonObject &profile);
  void show(wlr_scene_tree *tree, const QRect &geometry);
  void activate(wlr_scene_tree *tree, const QRect &geometry);
  void setGeometry(wlr_scene_tree *tree, const QRect &previous,
                   const QRect &geometry, wlr_scene_tree *overlay,
                   bool animate = true);
  QRect visualGeometry(wlr_scene_tree *tree, const QRect &fallback) const;
  void hideSnapshot(wlr_scene_tree *source, wlr_scene_tree *parent);
  void cancel(wlr_scene_tree *tree);
  void clear();
  int activeCount() const;
  void advance();

private:
  struct LiveState;
  struct SnapshotState;

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
