#include "compositor/window/animation/SceneAnimationBackend.hpp"
#include "compositor/wayland/wlroots/WlrootsSceneHeaders.hpp"
#include "core/templates/WaylandSlot.hpp"

#include <QEasingCurve>
#include <QElapsedTimer>
#include <algorithm>

namespace LunaDash {
namespace {
using OpacityMap = QHash<quintptr, float>;

struct OpacityContext {
  OpacityMap *base;
  qreal factor;
};

void applyOpacity(wlr_scene_buffer *buffer, int, int, void *data) {
  auto *context = static_cast<OpacityContext *>(data);
  const auto key = reinterpret_cast<quintptr>(buffer);
  if (!context->base->contains(key))
    context->base->insert(key, buffer->opacity);
  wlr_scene_buffer_set_opacity(
      buffer, std::clamp(context->base->value(key) *
                             static_cast<float>(context->factor),
                         0.0f, 1.0f));
}

struct SnapshotBuffer {
  wlr_scene_buffer *buffer;
  wlr_buffer *retained;
  QRect geometry;
  float opacity;
};

struct SnapshotBuildContext {
  wlr_scene_tree *parent;
  QPoint origin;
  QList<SnapshotBuffer> *buffers;
};

void cloneBuffer(wlr_scene_buffer *buffer, int sx, int sy, void *data) {
  auto *context = static_cast<SnapshotBuildContext *>(data);
  if (!buffer->buffer)
    return;
  auto *copy = wlr_scene_buffer_create(context->parent, buffer->buffer);
  if (!copy)
    return;
  // Visual previews never take pointer focus away from the real surfaces.
  copy->point_accepts_input = [](wlr_scene_buffer *, double *, double *) {
    return false;
  };
  QSize size(buffer->dst_width, buffer->dst_height);
  if (size.width() <= 0 || size.height() <= 0) {
    size = QSize(buffer->buffer->width, buffer->buffer->height);
    if (buffer->transform % 2)
      size.transpose();
  }
  const QPoint position = QPoint(sx, sy) - context->origin;
  wlr_scene_node_set_position(&copy->node, position.x(), position.y());
  wlr_scene_buffer_set_source_box(copy, &buffer->src_box);
  wlr_scene_buffer_set_dest_size(copy, size.width(), size.height());
  wlr_scene_buffer_set_transform(copy, buffer->transform);
  wlr_scene_buffer_set_opacity(copy, buffer->opacity);
  // Scene buffers may release their backing buffer after texture upload.
  // Hold it through this short transition so retarget/close can clone it again.
  context->buffers->append({copy, wlr_buffer_lock(buffer->buffer),
                            QRect(position, size), buffer->opacity});
}

QRect interpolate(const QRect &from, const QRect &to, qreal progress) {
  auto blend = [progress](int a, int b) {
    return qRound(a + (b - a) * progress);
  };
  return {blend(from.x(), to.x()), blend(from.y(), to.y()),
          std::max(1, blend(from.width(), to.width())),
          std::max(1, blend(from.height(), to.height()))};
}

qreal elapsedProgress(const QElapsedTimer &elapsed, int duration) {
  return std::clamp(static_cast<double>(elapsed.nsecsElapsed()) /
                        (std::max(1, duration) * 1000000.0),
                    0.0, 1.0);
}
} // namespace

struct SceneAnimationBackend::LiveState {
  SceneAnimationBackend *owner = nullptr;
  wlr_scene_tree *tree = nullptr;
  Templates::WaylandSlot<LiveState> destroy;
  QElapsedTimer elapsed;
  int duration = 0;
  QRect from;
  QRect target;
  QRect current;
  OpacityMap opacity;
  qreal initialOpacity = 1.0;
  qreal currentOpacity = 1.0;
  SnapshotState *preview = nullptr;
};

struct SceneAnimationBackend::SnapshotState {
  SceneAnimationBackend *owner = nullptr;
  LiveState *live = nullptr;
  wlr_scene_tree *tree = nullptr;
  Templates::WaylandSlot<SnapshotState> destroy;
  QElapsedTimer elapsed;
  int duration = 0;
  QRect original;
  QRect current;
  QPoint parentOrigin;
  QList<SnapshotBuffer> buffers;
  ~SnapshotState() {
    for (const auto &part : buffers)
      wlr_buffer_unlock(part.retained);
  }
};

SceneAnimationBackend::~SceneAnimationBackend() { clear(); }

void SceneAnimationBackend::setDuration(int milliseconds) {
  duration_ = std::clamp(milliseconds, 0, 600);
  if (duration_ == 0)
    clear();
}

void SceneAnimationBackend::configure(const QJsonObject &profile) {
  setDuration(profile.value("duration").toInt(220));
  enterOffset_ = std::clamp(profile.value("enterOffset").toInt(12), -100, 100);
  focusOpacity_ =
      std::clamp(profile.value("focusOpacity").toDouble(0.82), 0.0, 1.0);
  exitScale_ = std::clamp(profile.value("exitScale").toDouble(0.90), 0.5, 1.0);
  const auto curve = profile.value("easing").toString();
  easing_ = QEasingCurve(curve == "linear"       ? QEasingCurve::Linear
                         : curve == "outQuint"   ? QEasingCurve::OutQuint
                         : curve == "inOutCubic" ? QEasingCurve::InOutCubic
                                                 : QEasingCurve::OutCubic);
}

int SceneAnimationBackend::activeCount() const {
  int count = live_.size();
  for (const auto *snapshot : snapshots_)
    if (!snapshot->live)
      ++count;
  return count;
}

QRect SceneAnimationBackend::visualGeometry(wlr_scene_tree *tree,
                                            const QRect &fallback) const {
  if (const auto *state = live_.value(tree))
    return state->preview ? state->preview->current : state->current;
  return tree ? QRect(QPoint(tree->node.x, tree->node.y), fallback.size())
              : fallback;
}

void SceneAnimationBackend::startLive(LiveState *state) {
  state->owner = this;
  state->duration = duration_;
  state->elapsed.start();
  live_.insert(state->tree, state);
  Templates::attachListener(&state->tree->node.events.destroy, state->destroy,
                            state, [](wl_listener *listener, void *) {
                              auto *live =
                                  Templates::listenerOwner<LiveState>(listener);
                              live->owner->finishLive(live->tree, live, false);
                            });
  applyLive(state, 0.0);
}

void SceneAnimationBackend::applyLive(LiveState *state, qreal progress) {
  state->current = interpolate(state->from, state->target, progress);
  if (state->preview) {
    // Configure the client once at its final size. Fit the previous frame
    // without distortion, then reveal the newly laid-out content.
    const qreal reveal = std::clamp((progress - 0.72) / 0.28, 0.0, 1.0);
    applySnapshot(state->preview, state->current, 1.0 - reveal);
    state->currentOpacity = reveal;
    wlr_scene_node_set_position(&state->tree->node, state->target.x(),
                                state->target.y());
  } else {
    state->currentOpacity =
        state->initialOpacity + (1.0 - state->initialOpacity) * progress;
    wlr_scene_node_set_position(&state->tree->node, state->current.x(),
                                state->current.y());
  }
  if (state->preview || state->initialOpacity != 1.0) {
    OpacityContext context{&state->opacity, state->currentOpacity};
    wlr_scene_node_for_each_buffer(&state->tree->node, applyOpacity, &context);
  }
}

void SceneAnimationBackend::finishLive(wlr_scene_tree *tree, LiveState *state,
                                       bool restore) {
  if (live_.value(tree) != state)
    return;
  Templates::detachListener(state->destroy);
  live_.remove(tree);
  if (restore) {
    OpacityContext context{&state->opacity, 1.0};
    wlr_scene_node_for_each_buffer(&tree->node, applyOpacity, &context);
    wlr_scene_node_set_position(&tree->node, state->target.x(),
                                state->target.y());
  }
  if (state->preview)
    destroySnapshot(state->preview);
  delete state;
}

void SceneAnimationBackend::open(wlr_scene_tree *tree, const QRect &geometry) {
  if (!tree)
    return;
  cancel(tree);
  if (duration_ <= 0)
    return;
  auto *state = new LiveState;
  state->tree = tree;
  state->from = geometry.translated(0, enterOffset_);
  state->target = geometry;
  state->initialOpacity = 0.0;
  startLive(state);
}

void SceneAnimationBackend::focus(wlr_scene_tree *tree,
                                     const QRect &geometry) {
  if (!tree || duration_ <= 0 || live_.contains(tree))
    return;
  auto *state = new LiveState;
  state->tree = tree;
  state->from = geometry;
  state->target = geometry;
  state->initialOpacity = focusOpacity_;
  startLive(state);
}

void SceneAnimationBackend::relayout(wlr_scene_tree *tree,
                                        const QRect &previous,
                                        const QRect &geometry,
                                        wlr_scene_tree *overlay, bool animate) {
  if (!tree)
    return;
  if (!animate || duration_ <= 0 || !previous.isValid()) {
    cancel(tree);
    wlr_scene_node_set_position(&tree->node, geometry.x(), geometry.y());
    return;
  }
  auto *old = live_.value(tree);
  if ((old && old->target == geometry) || (!old && previous == geometry))
    return;
  const QRect from = visualGeometry(tree, previous);
  const qreal opacity = old ? old->currentOpacity : 1.0;
  SnapshotState *preview = nullptr;
  if (from.size() != geometry.size()) {
    if (old && old->preview)
      applySnapshot(old->preview, old->current, 1.0);
    else if (old) {
      OpacityContext context{&old->opacity, 1.0};
      wlr_scene_node_for_each_buffer(&tree->node, applyOpacity, &context);
    }
    auto *source = old && old->preview ? old->preview->tree : tree;
    preview = createSnapshot(source, overlay, from);
  }
  cancel(tree);
  auto *state = new LiveState;
  state->tree = tree;
  state->from = from;
  state->target = geometry;
  state->initialOpacity = preview ? 1.0 : opacity;
  state->preview = preview;
  if (preview)
    preview->live = state;
  startLive(state);
}

SceneAnimationBackend::SnapshotState *SceneAnimationBackend::createSnapshot(
    wlr_scene_tree *source, wlr_scene_tree *parent, const QRect &geometry) {
  if (!source || !parent)
    return nullptr;
  int sourceX = 0, sourceY = 0;
  if (!wlr_scene_node_coords(&source->node, &sourceX, &sourceY))
    return nullptr;
  auto *tree = wlr_scene_tree_create(parent);
  if (!tree)
    return nullptr;
  auto *state = new SnapshotState;
  state->owner = this;
  state->tree = tree;
  int parentX = 0, parentY = 0;
  wlr_scene_node_coords(&parent->node, &parentX, &parentY);
  state->parentOrigin = {parentX, parentY};
  SnapshotBuildContext context{tree, QPoint(sourceX, sourceY), &state->buffers};
  wlr_scene_node_for_each_buffer(&source->node, cloneBuffer, &context);
  if (state->buffers.isEmpty()) {
    wlr_scene_node_destroy(&tree->node);
    delete state;
    return nullptr;
  }
  QRect bounds;
  for (const auto &buffer : state->buffers)
    bounds = bounds.united(buffer.geometry);
  state->original = geometry.isValid()
                        ? geometry
                        : QRect(QPoint(sourceX, sourceY), bounds.size());
  state->current = state->original;
  state->duration = duration_;
  state->elapsed.start();
  snapshots_.append(state);
  Templates::attachListener(
      &tree->node.events.destroy, state->destroy, state,
      [](wl_listener *listener, void *) {
        auto *snapshot = Templates::listenerOwner<SnapshotState>(listener);
        Templates::detachListener(snapshot->destroy);
        snapshot->owner->snapshots_.removeAll(snapshot);
        if (snapshot->live)
          snapshot->live->preview = nullptr;
        delete snapshot;
      });
  applySnapshot(state, state->original, 1.0);
  return state;
}

void SceneAnimationBackend::applySnapshot(SnapshotState *state,
                                          const QRect &geometry,
                                          qreal opacity) {
  const qreal scale =
      std::min(qreal(geometry.width()) / state->original.width(),
               qreal(geometry.height()) / state->original.height());
  const QSize fitted(std::max(1, qRound(state->original.width() * scale)),
                     std::max(1, qRound(state->original.height() * scale)));
  state->current = QRect(geometry.topLeft() +
                             QPoint((geometry.width() - fitted.width()) / 2,
                                    (geometry.height() - fitted.height()) / 2),
                         fitted);
  const QPoint origin = state->current.topLeft() - state->parentOrigin;
  wlr_scene_node_set_position(&state->tree->node, origin.x(), origin.y());
  for (const auto &part : state->buffers) {
    wlr_scene_node_set_position(&part.buffer->node,
                                qRound(part.geometry.x() * scale),
                                qRound(part.geometry.y() * scale));
    wlr_scene_buffer_set_dest_size(
        part.buffer, std::max(1, qRound(part.geometry.width() * scale)),
        std::max(1, qRound(part.geometry.height() * scale)));
    wlr_scene_buffer_set_opacity(part.buffer, part.opacity * opacity);
  }
}

void SceneAnimationBackend::destroySnapshot(SnapshotState *state) {
  Templates::detachListener(state->destroy);
  snapshots_.removeAll(state);
  if (state->live)
    state->live->preview = nullptr;
  wlr_scene_node_destroy(&state->tree->node);
  delete state;
}

void SceneAnimationBackend::close(wlr_scene_tree *source,
                                         wlr_scene_tree *parent) {
  if (!source || duration_ <= 0)
    return;
  auto *live = live_.value(source);
  if (live && live->preview)
    source = live->preview->tree;
  createSnapshot(source, parent,
                 live ? visualGeometry(live->tree, live->current) : QRect());
}

void SceneAnimationBackend::advance() {
  // Follow output presentation cadence; no independent timer redraws idle
  // frames.
  const auto live = live_.values();
  for (auto *state : live) {
    const qreal t = elapsedProgress(state->elapsed, state->duration);
    if (t >= 1.0)
      finishLive(state->tree, state);
    else
      applyLive(state, easing_.valueForProgress(t));
  }
  const auto snapshots = snapshots_;
  for (auto *state : snapshots) {
    if (state->live)
      continue;
    const qreal t = elapsedProgress(state->elapsed, state->duration);
    if (t >= 1.0) {
      destroySnapshot(state);
      continue;
    }
    const qreal eased = QEasingCurve(QEasingCurve::InCubic).valueForProgress(t);
    const QRect &from = state->original;
    const QSize size(std::max(1, qRound(from.width() * exitScale_)),
                     std::max(1, qRound(from.height() * exitScale_)));
    const QRect target(from.x() + (from.width() - size.width()) / 2,
                       from.y() + (from.height() - size.height()) / 2 + 14,
                       size.width(), size.height());
    applySnapshot(state, interpolate(from, target, eased), 1.0 - eased);
  }
}

void SceneAnimationBackend::cancel(wlr_scene_tree *tree) {
  if (auto *state = live_.value(tree))
    finishLive(tree, state);
}

void SceneAnimationBackend::clear() {
  for (auto *tree : live_.keys())
    cancel(tree);
  const auto snapshots = snapshots_;
  for (auto *state : snapshots)
    destroySnapshot(state);
}
} // namespace LunaDash
