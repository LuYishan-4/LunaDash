#include "compositor/render/decorations/SceneWindowAnimations.hpp"
#include "compositor/wlroots/WlrootsSceneHeaders.hpp"

#include <QEasingCurve>
#include <QVariantAnimation>
#include <algorithm>
#include <cmath>

namespace LuDash {
namespace {

using OpacityMap = QHash<quintptr, float>;

struct OpacityContext {
  OpacityMap *base = nullptr;
  qreal factor = 1.0;
};

void applyOpacity(wlr_scene_buffer *buffer, int, int, void *data) {
  auto *context = static_cast<OpacityContext *>(data);
  const auto key = reinterpret_cast<quintptr>(buffer);
  if (!context->base->contains(key))
    context->base->insert(key, buffer->opacity);
  const float baseOpacity = context->base->value(key, 1.0f);
  wlr_scene_buffer_set_opacity(
      buffer, std::clamp(baseOpacity * static_cast<float>(context->factor),
                         0.0f, 1.0f));
}

void restoreOpacity(wlr_scene_tree *tree, OpacityMap &base) {
  if (!tree)
    return;
  OpacityContext context{&base, 1.0};
  wlr_scene_node_for_each_buffer(&tree->node, applyOpacity, &context);
}

struct SnapshotBuildContext {
  wlr_scene_tree *parent = nullptr;
  int parentX = 0;
  int parentY = 0;
  OpacityMap *base = nullptr;
  int count = 0;
};

void cloneBuffer(wlr_scene_buffer *buffer, int sx, int sy, void *data) {
  auto *context = static_cast<SnapshotBuildContext *>(data);
  if (!buffer->buffer)
    return;

  auto *copy = wlr_scene_buffer_create(context->parent, buffer->buffer);
  if (!copy)
    return;

  wlr_scene_node_set_position(&copy->node, sx - context->parentX,
                              sy - context->parentY);
  wlr_scene_buffer_set_source_box(copy, &buffer->src_box);
  wlr_scene_buffer_set_dest_size(copy, buffer->dst_width, buffer->dst_height);
  wlr_scene_buffer_set_transform(copy, buffer->transform);
  wlr_scene_buffer_set_opacity(copy, buffer->opacity);
  context->base->insert(reinterpret_cast<quintptr>(copy), buffer->opacity);
  ++context->count;
}

} // namespace

struct SceneWindowAnimations::LiveState {
  wlr_scene_tree *tree = nullptr;
  QVariantAnimation *animation = nullptr;
  QPoint basePosition;
  OpacityMap opacity;
  qreal progress = 0.0;
};

struct SceneWindowAnimations::SnapshotState {
  wlr_scene_tree *tree = nullptr;
  QVariantAnimation *animation = nullptr;
  OpacityMap opacity;
};

SceneWindowAnimations::SceneWindowAnimations(QObject *parent) : QObject(parent) {}

SceneWindowAnimations::~SceneWindowAnimations() { clear(); }

void SceneWindowAnimations::setDuration(int milliseconds) {
  const int next = std::clamp(milliseconds, 0, 600);
  if (duration_ == next)
    return;
  duration_ = next;
  if (duration_ != 0)
    return;

  const auto liveTrees = live_.keys();
  for (auto *tree : liveTrees)
    cancel(tree);

  const auto snapshots = snapshots_;
  for (auto *state : snapshots) {
    snapshots_.removeAll(state);
    if (state->animation) {
      state->animation->stop();
      state->animation->deleteLater();
    }
    if (state->tree)
      wlr_scene_node_destroy(&state->tree->node);
    delete state;
  }
}

int SceneWindowAnimations::activeCount() const {
  return live_.size() + snapshots_.size();
}

void SceneWindowAnimations::applyLive(LiveState *state, qreal progress) {
  if (!state || !state->tree)
    return;
  state->progress = std::clamp(progress, 0.0, 1.0);
  OpacityContext context{&state->opacity, state->progress};
  wlr_scene_node_for_each_buffer(&state->tree->node, applyOpacity, &context);
  const int lift =
      qRound((1.0 - state->progress) * std::min(10, duration_ / 25 + 3));
  wlr_scene_node_set_position(&state->tree->node, state->basePosition.x(),
                              state->basePosition.y() + lift);
}

void SceneWindowAnimations::finishLive(wlr_scene_tree *tree, LiveState *state) {
  if (!tree || !state || live_.value(tree) != state)
    return;
  restoreOpacity(tree, state->opacity);
  wlr_scene_node_set_position(&tree->node, state->basePosition.x(),
                              state->basePosition.y());
  live_.remove(tree);
  if (state->animation)
    state->animation->deleteLater();
  delete state;
}

void SceneWindowAnimations::show(wlr_scene_tree *tree, const QPoint &position) {
  if (!tree)
    return;

  cancel(tree);
  if (duration_ <= 0) {
    wlr_scene_node_set_position(&tree->node, position.x(), position.y());
    return;
  }

  auto *state = new LiveState;
  state->tree = tree;
  state->basePosition = position;
  state->animation = new QVariantAnimation(this);
  state->animation->setStartValue(0.0);
  state->animation->setEndValue(1.0);
  state->animation->setDuration(duration_);
  state->animation->setEasingCurve(QEasingCurve::OutCubic);
  live_.insert(tree, state);

  applyLive(state, 0.0);
  connect(state->animation, &QVariantAnimation::valueChanged, this,
          [this, state](const QVariant &value) {
            applyLive(state, value.toReal());
          });
  connect(state->animation, &QVariantAnimation::finished, this,
          [this, tree, state] { finishLive(tree, state); });
  state->animation->start();
}

void SceneWindowAnimations::setPosition(wlr_scene_tree *tree,
                                        const QPoint &position) {
  if (!tree)
    return;
  if (auto *state = live_.value(tree)) {
    state->basePosition = position;
    applyLive(state, state->progress);
    return;
  }
  wlr_scene_node_set_position(&tree->node, position.x(), position.y());
}

void SceneWindowAnimations::hideSnapshot(wlr_scene_tree *source,
                                         wlr_scene_tree *parent) {
  if (!source || !parent || duration_ <= 0)
    return;

  int sourceX = 0;
  int sourceY = 0;
  if (!wlr_scene_node_coords(&source->node, &sourceX, &sourceY))
    return;

  auto *snapshot = wlr_scene_tree_create(parent);
  if (!snapshot)
    return;

  int parentX = 0;
  int parentY = 0;
  wlr_scene_node_coords(&parent->node, &parentX, &parentY);

  auto *state = new SnapshotState;
  state->tree = snapshot;
  SnapshotBuildContext build{snapshot, parentX, parentY, &state->opacity, 0};
  wlr_scene_node_for_each_buffer(&source->node, cloneBuffer, &build);
  if (build.count == 0) {
    wlr_scene_node_destroy(&snapshot->node);
    delete state;
    return;
  }

  wlr_scene_node_raise_to_top(&snapshot->node);
  state->animation = new QVariantAnimation(this);
  state->animation->setStartValue(1.0);
  state->animation->setEndValue(0.0);
  state->animation->setDuration(duration_);
  state->animation->setEasingCurve(QEasingCurve::InCubic);
  snapshots_.append(state);

  connect(state->animation, &QVariantAnimation::valueChanged, this,
          [state](const QVariant &value) {
            OpacityContext context{&state->opacity, value.toReal()};
            wlr_scene_node_for_each_buffer(&state->tree->node, applyOpacity,
                                           &context);
            const int drop = qRound((1.0 - value.toReal()) * 8.0);
            wlr_scene_node_set_position(&state->tree->node, 0, drop);
          });
  connect(state->animation, &QVariantAnimation::finished, this, [this, state] {
    snapshots_.removeAll(state);
    if (state->tree)
      wlr_scene_node_destroy(&state->tree->node);
    if (state->animation)
      state->animation->deleteLater();
    delete state;
  });
  state->animation->start();
}

void SceneWindowAnimations::cancel(wlr_scene_tree *tree) {
  auto *state = live_.take(tree);
  if (!state)
    return;
  if (state->animation) {
    state->animation->stop();
    state->animation->deleteLater();
  }
  if (tree) {
    restoreOpacity(tree, state->opacity);
    wlr_scene_node_set_position(&tree->node, state->basePosition.x(),
                                state->basePosition.y());
  }
  delete state;
}

void SceneWindowAnimations::clear() {
  const auto trees = live_.keys();
  for (auto *tree : trees)
    cancel(tree);

  const auto snapshots = snapshots_;
  snapshots_.clear();
  for (auto *state : snapshots) {
    if (state->animation) {
      state->animation->stop();
      state->animation->deleteLater();
    }
    if (state->tree)
      wlr_scene_node_destroy(&state->tree->node);
    delete state;
  }
}

} // namespace LuDash
