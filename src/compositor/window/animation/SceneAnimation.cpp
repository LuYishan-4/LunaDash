#include "compositor/window/animation/SceneAnimation.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <utility>

#include "compositor/wayland/wlroots/WlrootsSceneHeaders.hpp"
#include "core/templates/WaylandSlot.hpp"

extern "C" {
#include <wlr/version.h>
#if WLR_VERSION_MINOR >= 19
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#endif
}

namespace LunaDash {
namespace {
using OpacityMap = std::unordered_map<wlr_scene_buffer *, float>;
using Clock = std::chrono::steady_clock;

struct OpacityContext {
  OpacityMap *base;
  double factor;
};

struct SnapshotBuffer {
  wlr_scene_buffer *buffer;
  wlr_buffer *retained;
  SceneAnimationRectangle geometry;
  float opacity;
};

struct SnapshotBuildContext {
  wlr_scene_tree *parent;
  int originX;
  int originY;
  std::vector<SnapshotBuffer> *buffers;
  bool synchronizedSource = false;
};

void applyOpacity(wlr_scene_buffer *buffer, int, int, void *data) {
  auto *context = static_cast<OpacityContext *>(data);
  const auto [entry, inserted] =
      context->base->try_emplace(buffer, buffer->opacity);
  (void)inserted;
  wlr_scene_buffer_set_opacity(
      buffer, std::clamp(entry->second * static_cast<float>(context->factor),
                         0.0f, 1.0f));
}

void cloneBuffer(wlr_scene_buffer *buffer, int sx, int sy, void *data) {
  auto *context = static_cast<SnapshotBuildContext *>(data);
  if (context->synchronizedSource)
    return;
#if WLR_VERSION_MINOR >= 19
  if (auto *surface = wlr_scene_surface_try_from_buffer(buffer)) {
    const auto *sync =
        wlr_linux_drm_syncobj_v1_get_surface_state(surface->surface);
    if (sync && sync->acquire_timeline) {
      // A detached preview outlives the surface commit. Copying only its
      // acquire wait would not hold the client's release point pending.
      // Keep live scene animation, but decline the optional snapshot until
      // it can own commit releases throughout every preview render pass.
      context->synchronizedSource = true;
      return;
    }
  }
#endif
  if (!buffer->buffer)
    return;
  auto *copy = wlr_scene_buffer_create(context->parent, buffer->buffer);
  if (!copy)
    return;
  // Visual previews never take pointer focus away from the real surfaces.
  copy->point_accepts_input = [](wlr_scene_buffer *, double *, double *) {
    return false;
  };

  int width = buffer->dst_width;
  int height = buffer->dst_height;
  if (width <= 0 || height <= 0) {
    width = buffer->buffer->width;
    height = buffer->buffer->height;
    if (buffer->transform % 2)
      std::swap(width, height);
  }
  const int positionX = sx - context->originX;
  const int positionY = sy - context->originY;
  wlr_scene_node_set_position(&copy->node, positionX, positionY);
  wlr_scene_buffer_set_source_box(copy, &buffer->src_box);
  wlr_scene_buffer_set_dest_size(copy, width, height);
  wlr_scene_buffer_set_transform(copy, buffer->transform);
  wlr_scene_buffer_set_opacity(copy, buffer->opacity);
  // Scene buffers may release their backing buffer after texture upload.
  // Hold it through this short transition so retarget/close can clone it again.
  context->buffers->push_back({copy,
                               wlr_buffer_lock(buffer->buffer),
                               {positionX, positionY, width, height},
                               buffer->opacity});
}

SceneAnimationRectangle interpolate(const SceneAnimationRectangle &from,
                                    const SceneAnimationRectangle &to,
                                    double progress) {
  const auto blend = [progress](int a, int b) {
    return static_cast<int>(std::lround(a + (b - a) * progress));
  };
  return {blend(from.left, to.left), blend(from.top, to.top),
          std::max(1, blend(from.width, to.width)),
          std::max(1, blend(from.height, to.height))};
}

SceneAnimationRectangle unite(const SceneAnimationRectangle &first,
                              const SceneAnimationRectangle &second) {
  if (!first.valid())
    return second;
  if (!second.valid())
    return first;
  const int left = std::min(first.left, second.left);
  const int top = std::min(first.top, second.top);
  const int right =
      std::max(first.left + first.width, second.left + second.width);
  const int bottom =
      std::max(first.top + first.height, second.top + second.height);
  return {left, top, right - left, bottom - top};
}

double elapsedProgress(Clock::time_point started, int durationMilliseconds) {
  const auto elapsed =
      std::chrono::duration<double, std::milli>(Clock::now() - started).count();
  return std::clamp(elapsed / std::max(1, durationMilliseconds), 0.0, 1.0);
}

double ease(SceneAnimationCurve curve, double progress) {
  switch (curve) {
  case SceneAnimationCurve::Linear:
    return progress;
  case SceneAnimationCurve::OutQuint:
    return 1.0 - std::pow(1.0 - progress, 5.0);
  case SceneAnimationCurve::InOutCubic:
    return progress < 0.5 ? 4.0 * progress * progress * progress
                          : 1.0 - std::pow(-2.0 * progress + 2.0, 3.0) / 2.0;
  case SceneAnimationCurve::OutCubic:
  default:
    return 1.0 - std::pow(1.0 - progress, 3.0);
  }
}
} // namespace

struct SceneAnimation::LiveState {
  SceneAnimation *owner = nullptr;
  wlr_scene_tree *tree = nullptr;
  Templates::WaylandSlot<LiveState> destroy;
  Clock::time_point started;
  int duration = 0;
  SceneAnimationRectangle from;
  SceneAnimationRectangle target;
  SceneAnimationRectangle current;
  OpacityMap opacity;
  double initialOpacity = 1.0;
  double currentOpacity = 1.0;
  SnapshotState *preview = nullptr;
};

struct SceneAnimation::SnapshotState {
  SceneAnimation *owner = nullptr;
  LiveState *live = nullptr;
  wlr_scene_tree *tree = nullptr;
  Templates::WaylandSlot<SnapshotState> destroy;
  Clock::time_point started;
  int duration = 0;
  SceneAnimationRectangle original;
  SceneAnimationRectangle current;
  int parentOriginX = 0;
  int parentOriginY = 0;
  bool slide = false;
  bool fade = true;
  int slideX = 0;
  int slideY = 0;
  std::vector<SnapshotBuffer> buffers;
  ~SnapshotState() {
    for (const auto &part : buffers)
      wlr_buffer_unlock(part.retained);
  }
};

SceneAnimation::~SceneAnimation() { clear(); }

void SceneAnimation::setDuration(int milliseconds) {
  duration_ = std::clamp(milliseconds, 0, 600);
  if (duration_ == 0)
    clear();
}

void SceneAnimation::configure(const SceneAnimationOptions &options) {
  setDuration(options.duration);
  enterOffset_ = std::clamp(options.enterOffset, -100, 100);
  focusOpacity_ = std::clamp(options.focusOpacity, 0.0, 1.0);
  exitScale_ = std::clamp(options.exitScale, 0.5, 1.0);
  curve_ = options.curve;
}

int SceneAnimation::activeCount() const {
  int count = static_cast<int>(live_.size());
  for (const auto *snapshot : snapshots_)
    if (!snapshot->live)
      ++count;
  return count;
}

SceneAnimationRectangle
SceneAnimation::visualGeometry(wlr_scene_tree *tree,
                               const SceneAnimationRectangle &fallback) const {
  const auto found = live_.find(tree);
  if (found != live_.end()) {
    const auto *state = found->second;
    return state->preview ? state->preview->current : state->current;
  }
  return tree ? SceneAnimationRectangle{tree->node.x, tree->node.y,
                                        fallback.width, fallback.height}
              : fallback;
}

void SceneAnimation::startLive(LiveState *state) {
  state->owner = this;
  state->duration = duration_;
  state->started = Clock::now();
  live_.insert_or_assign(state->tree, state);
  Templates::attachListener(&state->tree->node.events.destroy, state->destroy,
                            state, [](wl_listener *listener, void *) {
                              auto *live =
                                  Templates::listenerOwner<LiveState>(listener);
                              live->owner->finishLive(live->tree, live, false);
                            });
  applyLive(state, 0.0);
}

void SceneAnimation::applyLive(LiveState *state, double progress) {
  state->current = interpolate(state->from, state->target, progress);
  if (state->preview) {
    // Configure the client once at its final size. Move the previous frame
    // through the interpolated tile while the newly laid-out content fades in.
    const double reveal = std::clamp((progress - 0.36) / 0.64, 0.0, 1.0);
    applySnapshot(state->preview, state->current, 1.0 - reveal);
    state->currentOpacity = reveal;
    wlr_scene_node_set_position(&state->tree->node, state->target.left,
                                state->target.top);
  } else {
    state->currentOpacity =
        state->initialOpacity + (1.0 - state->initialOpacity) * progress;
    wlr_scene_node_set_position(&state->tree->node, state->current.left,
                                state->current.top);
  }
  if (state->preview || state->initialOpacity != 1.0) {
    OpacityContext context{&state->opacity, state->currentOpacity};
    wlr_scene_node_for_each_buffer(&state->tree->node, applyOpacity, &context);
  }
}

void SceneAnimation::finishLive(wlr_scene_tree *tree, LiveState *state,
                                bool restore) {
  const auto found = live_.find(tree);
  if (found == live_.end() || found->second != state)
    return;
  Templates::detachListener(state->destroy);
  live_.erase(found);
  if (restore) {
    OpacityContext context{&state->opacity, 1.0};
    wlr_scene_node_for_each_buffer(&tree->node, applyOpacity, &context);
    wlr_scene_node_set_position(&tree->node, state->target.left,
                                state->target.top);
  }
  if (state->preview)
    destroySnapshot(state->preview);
  delete state;
}

void SceneAnimation::open(wlr_scene_tree *tree,
                          const SceneAnimationRectangle &geometry) {
  open(tree, geometry, 0, enterOffset_);
}

void SceneAnimation::open(wlr_scene_tree *tree,
                          const SceneAnimationRectangle &geometry, int dx,
                          int dy, bool fade) {
  if (!tree)
    return;
  cancel(tree);
  if (duration_ <= 0)
    return;
  auto *state = new LiveState;
  state->tree = tree;
  state->from = geometry.translated(dx, dy);
  state->target = geometry;
  state->initialOpacity = fade ? 0.0 : 1.0;
  startLive(state);
}

void SceneAnimation::focus(wlr_scene_tree *tree,
                           const SceneAnimationRectangle &geometry) {
  if (!tree || duration_ <= 0 || live_.contains(tree))
    return;
  auto *state = new LiveState;
  state->tree = tree;
  state->from = geometry;
  state->target = geometry;
  state->initialOpacity = focusOpacity_;
  startLive(state);
}

void SceneAnimation::relayout(wlr_scene_tree *tree,
                              const SceneAnimationRectangle &previous,
                              const SceneAnimationRectangle &geometry,
                              wlr_scene_tree *overlay, bool animate) {
  if (!tree)
    return;
  if (!animate || duration_ <= 0 || !previous.valid()) {
    cancel(tree);
    wlr_scene_node_set_position(&tree->node, geometry.left, geometry.top);
    return;
  }
  const auto found = live_.find(tree);
  auto *old = found == live_.end() ? nullptr : found->second;
  if ((old && old->target == geometry) || (!old && previous == geometry))
    return;
  const auto from = visualGeometry(tree, previous);
  const double opacity = old ? old->currentOpacity : 1.0;
  SnapshotState *preview = nullptr;
  if (!from.sameSize(geometry)) {
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

SceneAnimation::SnapshotState *
SceneAnimation::createSnapshot(wlr_scene_tree *source, wlr_scene_tree *parent,
                               const SceneAnimationRectangle &geometry) {
  if (!source || !parent)
    return nullptr;
  int sourceX = 0;
  int sourceY = 0;
  if (!wlr_scene_node_coords(&source->node, &sourceX, &sourceY))
    return nullptr;
  auto *tree = wlr_scene_tree_create(parent);
  if (!tree)
    return nullptr;
  auto *state = new SnapshotState;
  state->owner = this;
  state->tree = tree;
  int parentX = 0;
  int parentY = 0;
  wlr_scene_node_coords(&parent->node, &parentX, &parentY);
  state->parentOriginX = parentX;
  state->parentOriginY = parentY;
  SnapshotBuildContext context{tree, sourceX, sourceY, &state->buffers};
  wlr_scene_node_for_each_buffer(&source->node, cloneBuffer, &context);
  if (context.synchronizedSource || state->buffers.empty()) {
    wlr_scene_node_destroy(&tree->node);
    delete state;
    return nullptr;
  }
  SceneAnimationRectangle bounds;
  for (const auto &buffer : state->buffers)
    bounds = unite(bounds, buffer.geometry);
  state->original = geometry.valid()
                        ? geometry
                        : SceneAnimationRectangle{sourceX, sourceY,
                                                  bounds.width, bounds.height};
  state->current = state->original;
  state->duration = duration_;
  state->started = Clock::now();
  snapshots_.push_back(state);
  Templates::attachListener(
      &tree->node.events.destroy, state->destroy, state,
      [](wl_listener *listener, void *) {
        auto *snapshot = Templates::listenerOwner<SnapshotState>(listener);
        Templates::detachListener(snapshot->destroy);
        std::erase(snapshot->owner->snapshots_, snapshot);
        if (snapshot->live)
          snapshot->live->preview = nullptr;
        delete snapshot;
      });
  applySnapshot(state, state->original, 1.0);
  return state;
}

void SceneAnimation::applySnapshot(SnapshotState *state,
                                   const SceneAnimationRectangle &geometry,
                                   double opacity) {
  const double scaleX =
      static_cast<double>(geometry.width) / std::max(1, state->original.width);
  const double scaleY = static_cast<double>(geometry.height) /
                        std::max(1, state->original.height);
  // Relayout previews deliberately stretch for a few frames instead of
  // preserving aspect ratio with moving letterbox bars. The live client is
  // already configured at the destination size and cross-fades continuously.
  state->current = geometry;
  wlr_scene_node_set_position(&state->tree->node,
                              geometry.left - state->parentOriginX,
                              geometry.top - state->parentOriginY);
  for (const auto &part : state->buffers) {
    wlr_scene_node_set_position(
        &part.buffer->node,
        static_cast<int>(std::lround(part.geometry.left * scaleX)),
        static_cast<int>(std::lround(part.geometry.top * scaleY)));
    wlr_scene_buffer_set_dest_size(
        part.buffer,
        std::max(1,
                 static_cast<int>(std::lround(part.geometry.width * scaleX))),
        std::max(1,
                 static_cast<int>(std::lround(part.geometry.height * scaleY))));
    wlr_scene_buffer_set_opacity(part.buffer, part.opacity * opacity);
  }
}

void SceneAnimation::destroySnapshot(SnapshotState *state) {
  Templates::detachListener(state->destroy);
  std::erase(snapshots_, state);
  if (state->live)
    state->live->preview = nullptr;
  wlr_scene_node_destroy(&state->tree->node);
  delete state;
}

void SceneAnimation::close(wlr_scene_tree *source, wlr_scene_tree *parent) {
  close(source, parent, 0, 0);
}

void SceneAnimation::close(wlr_scene_tree *source, wlr_scene_tree *parent,
                           int dx, int dy, bool fade) {
  if (!source || duration_ <= 0)
    return;
  const auto found = live_.find(source);
  auto *live = found == live_.end() ? nullptr : found->second;
  if (live && live->preview)
    source = live->preview->tree;
  auto *snapshot =
      createSnapshot(source, parent,
                     live ? visualGeometry(live->tree, live->current)
                          : SceneAnimationRectangle{});
  if (snapshot && (dx || dy)) {
    snapshot->slide = true;
    snapshot->fade = fade;
    snapshot->slideX = dx;
    snapshot->slideY = dy;
  }
}

void SceneAnimation::retarget(wlr_scene_tree *tree,
                              const SceneAnimationRectangle &geometry) {
  const auto found = live_.find(tree);
  if (found == live_.end() || found->second->preview)
    return;
  auto *state = found->second;
  if (state->target == geometry)
    return;
  state->from = geometry.translated(state->from.left - state->target.left,
                                    state->from.top - state->target.top);
  state->target = geometry;
  applyLive(state,
            ease(curve_, elapsedProgress(state->started, state->duration)));
}

void SceneAnimation::advance() {
  // Follow output presentation cadence; no independent timer redraws idle
  // frames.
  std::vector<LiveState *> live;
  live.reserve(live_.size());
  for (const auto &[tree, state] : live_) {
    (void)tree;
    live.push_back(state);
  }
  for (auto *state : live) {
    const double progress = elapsedProgress(state->started, state->duration);
    if (progress >= 1.0)
      finishLive(state->tree, state);
    else
      applyLive(state, ease(curve_, progress));
  }

  const auto snapshots = snapshots_;
  for (auto *state : snapshots) {
    if (state->live)
      continue;
    const double progress = elapsedProgress(state->started, state->duration);
    if (progress >= 1.0) {
      destroySnapshot(state);
      continue;
    }
    const auto &from = state->original;
    if (state->slide) {
      const double moved = ease(curve_, progress);
      applySnapshot(state,
                    from.translated(
                        static_cast<int>(std::lround(state->slideX * moved)),
                        static_cast<int>(std::lround(state->slideY * moved))),
                    state->fade ? 1.0 - moved : 1.0);
      continue;
    }
    const double eased = progress * progress * progress;
    const int width =
        std::max(1, static_cast<int>(std::lround(from.width * exitScale_)));
    const int height =
        std::max(1, static_cast<int>(std::lround(from.height * exitScale_)));
    const SceneAnimationRectangle target{
        from.left + (from.width - width) / 2,
        from.top + (from.height - height) / 2 + 14, width, height};
    applySnapshot(state, interpolate(from, target, eased), 1.0 - eased);
  }
}

void SceneAnimation::cancel(wlr_scene_tree *tree) {
  const auto found = live_.find(tree);
  if (found != live_.end())
    finishLive(tree, found->second);
}

void SceneAnimation::clear() {
  std::vector<wlr_scene_tree *> trees;
  trees.reserve(live_.size());
  for (const auto &[tree, state] : live_) {
    (void)state;
    trees.push_back(tree);
  }
  for (auto *tree : trees)
    cancel(tree);
  const auto snapshots = snapshots_;
  for (auto *state : snapshots)
    destroySnapshot(state);
}
} // namespace LunaDash
