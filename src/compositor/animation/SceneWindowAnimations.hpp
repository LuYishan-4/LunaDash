#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPoint>
#include <functional>

struct wlr_scene_tree;
class QVariantAnimation;

namespace LunaDash {

class SceneWindowAnimations final : public QObject {
public:
  explicit SceneWindowAnimations(QObject *parent = nullptr);
  ~SceneWindowAnimations() override;

  void setDuration(int milliseconds);
  void show(wlr_scene_tree *tree, const QPoint &position);
  void setPosition(wlr_scene_tree *tree, const QPoint &position);
  void hideSnapshot(wlr_scene_tree *source, wlr_scene_tree *parent);
  void cancel(wlr_scene_tree *tree);
  void clear();
  int activeCount() const;

private:
  struct LiveState;
  struct SnapshotState;

  void applyLive(LiveState *state, qreal progress);
  void finishLive(wlr_scene_tree *tree, LiveState *state);

  QHash<wlr_scene_tree *, LiveState *> live_;
  QList<SnapshotState *> snapshots_;
  int duration_ = 220;
};

} // namespace LunaDash
