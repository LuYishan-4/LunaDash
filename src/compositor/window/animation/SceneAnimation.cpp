#include "compositor/window/animation/SceneAnimation.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <utility>

#include "compositor/wayland/wlroots/WlrootsSceneHeaders.hpp"
#include "core/templates/LunaClint.hpp"

extern "C" {
#include <wlr/version.h>
#if WLR_VERSION_MINOR >= 19
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#endif
}

namespace LunaDash {
namespace {
using OpacityMap = std::unordered_map<wlr_scene_buffer *, float>;
// CLOCK_MONOTONIC, the clock of output frame and presentation times.
using Clock = std::chrono::steady_clock;

// A frame that took longer than this does not advance springs further.
constexpr double maximumStepSeconds = 0.1;
// The crossfade from a relayout preview to the live window starts after this
// share of the motion; an opening window is revealed later.
constexpr double relayoutRevealStart = 0.4;
constexpr double openRevealStart = 0.6;
// A closing window sinks by this much while it shrinks.
constexpr int closeDrop = 14;

struct OpacityContext {
  OpacityMap *base;
  double factor;
};

struct SnapshotBuffer {
  wlr_scene_buffer *buffer;
  wlr_buffer *retained;
  SceneAnimationRectangle geometry;
  wlr_fbox source;
  wl_output_transform transform;
  float opacity;
};

struct SnapshotBuildContext {
  wlr_scene_tree *parent;
  int originX;
  int originY;
  std::vector<SnapshotBuffer> *buffers;
  bool synchronizedSource = false;
};

std::uint64_t monotonicNow() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          Clock::now().time_since_epoch())
          .count());
}

double secondsBetween(std::uint64_t from, std::uint64_t to) {
  return to > from ? static_cast<double>(to - from) / 1e9 : 0.0;
}

double timeProgress(std::uint64_t started, std::uint64_t now,
                    int durationMilliseconds) {
  if (durationMilliseconds <= 0)
    return 1.0;
  return std::clamp(secondsBetween(started, now) * 1000.0 / durationMilliseconds,
                    0.0, 1.0);
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


SceneSpring sanitized(const SceneSpring &spring) {
  return {std::clamp(spring.stiffness, 1.0, 20000.0),
          std::clamp(spring.dampingRatio, 0.1, 3.0),
          std::clamp(spring.restDistance, 0.01, 4.0),
          std::clamp(spring.restSpeed, 0.1, 200.0)};
}

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
                               buffer->src_box,
                               buffer->transform,
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

SceneAnimationRectangle intersect(const SceneAnimationRectangle &first,
                                  const SceneAnimationRectangle &second) {
  const int left = std::max(first.left, second.left);
  const int top = std::max(first.top, second.top);
  const int right =
      std::min(first.left + first.width, second.left + second.width);
  const int bottom =
      std::min(first.top + first.height, second.top + second.height);
  if (right <= left || bottom <= top)
    return {};
  return {left, top, right - left, bottom - top};
}

SceneAnimationRectangle scaledAroundCenter(const SceneAnimationRectangle &rect,
                                           double scale) {
  const int width = std::max(1, static_cast<int>(std::lround(rect.width * scale)));
  const int height =
      std::max(1, static_cast<int>(std::lround(rect.height * scale)));
  return {rect.left + (rect.width - width) / 2,
          rect.top + (rect.height - height) / 2, width, height};
}
} // namespace

// Exact solution of x'' = -k x - 2 zeta sqrt(k) x' over `seconds`, so the
// result does not depend on the frame rate.
void stepSceneSpring(double &offset, double &velocity, const SceneSpring &spring,
                double seconds) {
  if (seconds <= 0.0)
    return;
  const double omega = std::sqrt(std::max(1e-6, spring.stiffness));
  const double zeta = std::max(0.0, spring.dampingRatio);
  const double x = offset;
  const double v = velocity;
  if (std::abs(zeta - 1.0) < 1e-6) {
    const double b = v + omega * x;
    const double decay = std::exp(-omega * seconds);
    offset = (x + b * seconds) * decay;
    velocity = (b - omega * (x + b * seconds)) * decay;
  } else if (zeta < 1.0) {
    const double damped = omega * std::sqrt(1.0 - zeta * zeta);
    const double b = (v + zeta * omega * x) / damped;
    const double decay = std::exp(-zeta * omega * seconds);
    const double cosine = std::cos(damped * seconds);
    const double sine = std::sin(damped * seconds);
    offset = decay * (x * cosine + b * sine);
    velocity = decay * ((b * damped - zeta * omega * x) * cosine -
                        (x * damped + zeta * omega * b) * sine);
  } else {
    const double root = omega * std::sqrt(zeta * zeta - 1.0);
    const double r1 = -zeta * omega + root;
    const double r2 = -zeta * omega - root;
    const double c1 = (v - r2 * x) / (r1 - r2);
    const double c2 = x - c1;
    const double e1 = std::exp(r1 * seconds);
    const double e2 = std::exp(r2 * seconds);
    offset = c1 * e1 + c2 * e2;
    velocity = c1 * r1 * e1 + c2 * r2 * e2;
  }
}

void SceneAnimation::Motion::start(const SceneSpring &parameters,
                                   const SceneAnimationRectangle &from,
                                   const SceneAnimationRectangle &to) {
  spring = parameters;
  target = to;
  offset = {static_cast<double>(from.left - to.left),
            static_cast<double>(from.top - to.top),
            static_cast<double>(from.width - to.width),
            static_cast<double>(from.height - to.height)};
  velocity = {};
  initialDistance = 0.0;
  for (const double value : offset)
    initialDistance = std::max(initialDistance, std::abs(value));
}

void SceneAnimation::Motion::retarget(const SceneAnimationRectangle &to) {
  offset[0] += target.left - to.left;
  offset[1] += target.top - to.top;
  offset[2] += target.width - to.width;
  offset[3] += target.height - to.height;
  target = to;
  initialDistance = 0.0;
  for (const double value : offset)
    initialDistance = std::max(initialDistance, std::abs(value));
}

void SceneAnimation::Motion::step(double seconds) {
  for (std::size_t axis = 0; axis < offset.size(); ++axis)
    stepSceneSpring(offset[axis], velocity[axis], spring, seconds);
}

bool SceneAnimation::Motion::settled() const {
  for (std::size_t axis = 0; axis < offset.size(); ++axis)
    if (std::abs(offset[axis]) >= spring.restDistance ||
        std::abs(velocity[axis]) >= spring.restSpeed)
      return false;
  return true;
}

double SceneAnimation::Motion::progress() const {
  if (initialDistance <= 0.0)
    return 1.0;
  double remaining = 0.0;
  for (const double value : offset)
    remaining = std::max(remaining, std::abs(value));
  return std::clamp(1.0 - remaining / initialDistance, 0.0, 1.0);
}

SceneAnimationRectangle SceneAnimation::Motion::current() const {
  const auto at = [this](std::size_t axis, int base) {
    return base + static_cast<int>(std::lround(offset[axis]));
  };
  return {at(0, target.left), at(1, target.top),
          std::max(1, at(2, target.width)), std::max(1, at(3, target.height))};
}

struct SceneAnimation::LiveState : Templates::LunaClint<LiveState> {
  void watchTree() {
    listen<&LiveState::onTreeDestroy>(&tree->node.events.destroy, destroy);
  }
  void onTreeDestroy() { owner->finishLive(tree, this, false); }

  SceneAnimation *owner = nullptr;
  wlr_scene_tree *tree = nullptr;
  Templates::WaylandSlot<LiveState> destroy;
  Motion motion;
  std::uint64_t started = 0;
  std::uint64_t lastStep = 0;
  // The fade from initialOpacity (or of an opening snapshot); 0 for none.
  int fadeDuration = 0;
  SceneAnimationRectangle current;
  OpacityMap opacity;
  double initialOpacity = 1.0;
  double currentOpacity = 1.0;
  // How far the live window has replaced its preview; never decreases.
  double reveal = 0.0;
  // The preview is the opening window's snapshot (scaled), not a relayout
  // preview (cropped).
  bool opening = false;
  // slideOut(): the window leaves `home` and its node is disabled once settled.
  bool hiding = false;
  SceneAnimationRectangle home;
  SnapshotState *preview = nullptr;
};

struct SceneAnimation::SnapshotState : Templates::LunaClint<SnapshotState> {
  void watchTree() {
    listen<&SnapshotState::onTreeDestroy>(&tree->node.events.destroy, destroy);
  }
  void onTreeDestroy() {
    std::erase(owner->snapshots_, this);
    if (live)
      live->preview = nullptr;
    delete this;
  }

  SceneAnimation *owner = nullptr;
  LiveState *live = nullptr;
  wlr_scene_tree *tree = nullptr;
  Templates::WaylandSlot<SnapshotState> destroy;
  std::uint64_t started = 0;
  std::uint64_t lastStep = 0;
  SceneAnimationRectangle original;
  SceneAnimationRectangle current;
  int parentOriginX = 0;
  int parentOriginY = 0;
  // Cropped to the animated rectangle instead of scaled into it, so text in
  // a resizing window keeps its proportions.
  bool crop = false;
  // A closing snapshot sliding on a spring (workspace changes, layers).
  bool moving = false;
  bool fade = true;
  Motion motion;
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
  closeDuration_ = std::clamp(options.closeDuration, 0, 600);
  enterOffset_ = std::clamp(options.enterOffset, -100, 100);
  focusOpacity_ = std::clamp(options.focusOpacity, 0.0, 1.0);
  exitScale_ = std::clamp(options.exitScale, 0.5, 1.0);
  openScale_ = std::clamp(options.openScale, 0.5, 1.0);
  curve_ = options.curve;
  windowOpen_ = sanitized(options.windowOpen);
  windowMovement_ = sanitized(options.windowMovement);
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
  state->started = monotonicNow();
  state->lastStep = state->started;
  live_.insert_or_assign(state->tree, state);
  state->watchTree();
  applyLive(state, state->started);
}

void SceneAnimation::applyLive(LiveState *state, std::uint64_t nowNs) {
  state->current = state->motion.current();
  const double faded =
      ease(curve_, timeProgress(state->started, nowNs, state->fadeDuration));
  if (state->preview) {
    // The client is configured once at its final size. Its previous frame
    // moves through the animated rectangle while the live window fades in.
    const double begin = state->opening ? openRevealStart : relayoutRevealStart;
    const double reveal =
        state->motion.settled()
            ? 1.0
            : std::clamp((state->motion.progress() - begin) / (1.0 - begin),
                         0.0, 1.0);
    state->reveal = std::max(state->reveal, reveal);
    double previewOpacity = 1.0 - state->reveal;
    if (state->opening)
      previewOpacity *= faded;
    applySnapshot(state->preview, state->current, previewOpacity);
    state->currentOpacity = state->reveal;
    wlr_scene_node_set_position(&state->tree->node, state->motion.target.left,
                                state->motion.target.top);
  } else {
    state->currentOpacity =
        state->initialOpacity + (1.0 - state->initialOpacity) * faded;
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
    // A window that slid out returns to its place, hidden or about to be shown.
    const auto &position = state->hiding ? state->home : state->motion.target;
    wlr_scene_node_set_position(&tree->node, position.left, position.top);
  }
  if (state->preview)
    destroySnapshot(state->preview);
  delete state;
}

void SceneAnimation::open(wlr_scene_tree *tree,
                          const SceneAnimationRectangle &geometry) {
  if (!tree)
    return;
  cancel(tree);
  if (duration_ <= 0)
    return;
  if (openScale_ < 1.0 && openWithSnapshot(tree, geometry))
    return;
  open(tree, geometry, 0, enterOffset_);
}

bool SceneAnimation::openWithSnapshot(wlr_scene_tree *tree,
                                      const SceneAnimationRectangle &geometry) {
  if (!snapshotLayer_ || !geometry.valid())
    return false;
  auto *preview = createSnapshot(tree, snapshotLayer_, geometry);
  if (!preview)
    return false;
  auto *state = new LiveState;
  state->tree = tree;
  state->motion.start(windowOpen_, scaledAroundCenter(geometry, openScale_),
                      geometry);
  state->fadeDuration = duration_;
  state->initialOpacity = 0.0;
  state->opening = true;
  state->preview = preview;
  preview->live = state;
  startLive(state);
  return true;
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
  state->motion.start(fade ? windowOpen_ : windowMovement_,
                      geometry.translated(dx, dy), geometry);
  state->initialOpacity = fade ? 0.0 : 1.0;
  state->fadeDuration = fade ? duration_ : 0;
  startLive(state);
}

void SceneAnimation::focus(wlr_scene_tree *tree,
                           const SceneAnimationRectangle &geometry) {
  // A focus opacity of 1 means no focus feedback here; the shell and window
  // decorations show focus.
  if (!tree || duration_ <= 0 || focusOpacity_ >= 1.0 || live_.contains(tree))
    return;
  auto *state = new LiveState;
  state->tree = tree;
  state->motion.start(windowMovement_, geometry, geometry);
  state->initialOpacity = focusOpacity_;
  state->fadeDuration = duration_;
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
  if ((old && old->motion.target == geometry) || (!old && previous == geometry))
    return;
  const auto from = visualGeometry(tree, previous);
  const bool resize = !from.sameSize(geometry);

  if (old) {
    // Interrupted: continue from the current position with the current
    // velocity instead of starting over.
    if (resize && !old->preview) {
      OpacityContext context{&old->opacity, 1.0};
      wlr_scene_node_for_each_buffer(&tree->node, applyOpacity, &context);
      if (auto *preview = createSnapshot(tree, overlay, old->current)) {
        preview->crop = true;
        preview->live = old;
        old->preview = preview;
        old->reveal = 0.0;
        old->initialOpacity = 1.0;
        old->fadeDuration = 0;
      }
    }
    if (!old->opening)
      old->motion.spring = windowMovement_;
    old->motion.retarget(geometry);
    applyLive(old, monotonicNow());
    return;
  }

  SnapshotState *preview = nullptr;
  if (resize) {
    preview = createSnapshot(tree, overlay, from);
    if (preview)
      preview->crop = true;
  }
  auto *state = new LiveState;
  state->tree = tree;
  state->motion.start(windowMovement_, from, geometry);
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
  state->started = monotonicNow();
  state->lastStep = state->started;
  snapshots_.push_back(state);
  state->watchTree();
  applySnapshot(state, state->original, 1.0);
  return state;
}

void SceneAnimation::applySnapshot(SnapshotState *state,
                                   const SceneAnimationRectangle &geometry,
                                   double opacity) {
  state->current = geometry;
  wlr_scene_node_set_position(&state->tree->node,
                              geometry.left - state->parentOriginX,
                              geometry.top - state->parentOriginY);
  if (state->crop) {
    // Show the previous frame at its own size, cut to the animated rectangle.
    const SceneAnimationRectangle visibleArea{0, 0, geometry.width,
                                              geometry.height};
    for (const auto &part : state->buffers) {
      const auto visible = intersect(part.geometry, visibleArea);
      wlr_scene_node_set_enabled(&part.buffer->node, visible.valid());
      if (!visible.valid())
        continue;
      wlr_scene_buffer_set_opacity(part.buffer, part.opacity * opacity);
      wlr_scene_node_set_position(&part.buffer->node, visible.left, visible.top);
      wlr_scene_buffer_set_dest_size(part.buffer, visible.width, visible.height);
      // Rotated buffers keep their full source; only plain ones are cropped.
      if (part.transform != WL_OUTPUT_TRANSFORM_NORMAL)
        continue;
      wlr_fbox source = part.source;
      if (source.width <= 0 || source.height <= 0)
        source = {0.0, 0.0, static_cast<double>(part.retained->width),
                  static_cast<double>(part.retained->height)};
      const double scaleX = source.width / std::max(1, part.geometry.width);
      const double scaleY = source.height / std::max(1, part.geometry.height);
      const wlr_fbox cropped{
          source.x + (visible.left - part.geometry.left) * scaleX,
          source.y + (visible.top - part.geometry.top) * scaleY,
          visible.width * scaleX, visible.height * scaleY};
      wlr_scene_buffer_set_source_box(part.buffer, &cropped);
    }
    return;
  }
  const double scaleX =
      static_cast<double>(geometry.width) / std::max(1, state->original.width);
  const double scaleY = static_cast<double>(geometry.height) /
                        std::max(1, state->original.height);
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
  if (!snapshot || (!dx && !dy))
    return;
  snapshot->moving = true;
  snapshot->fade = fade;
  snapshot->motion.start(fade ? windowOpen_ : windowMovement_,
                         snapshot->original,
                         snapshot->original.translated(dx, dy));
}

bool SceneAnimation::slideOut(wlr_scene_tree *tree,
                              const SceneAnimationRectangle &geometry, int dx,
                              int dy) {
  if (!tree || duration_ <= 0 || (!dx && !dy))
    return false;
  const auto from = visualGeometry(tree, geometry);
  cancel(tree);
  auto *state = new LiveState;
  state->tree = tree;
  state->hiding = true;
  state->home = geometry;
  state->motion.start(windowMovement_, from, from.translated(dx, dy));
  startLive(state);
  return true;
}

bool SceneAnimation::slidingOut(wlr_scene_tree *tree) const {
  const auto found = live_.find(tree);
  return found != live_.end() && found->second->hiding;
}

void SceneAnimation::returnHome(wlr_scene_tree *tree) {
  const auto found = live_.find(tree);
  if (found == live_.end() || !found->second->hiding)
    return;
  auto *state = found->second;
  state->hiding = false;
  state->motion.retarget(state->home);
  applyLive(state, monotonicNow());
}

void SceneAnimation::retarget(wlr_scene_tree *tree,
                              const SceneAnimationRectangle &geometry) {
  const auto found = live_.find(tree);
  if (found == live_.end())
    return;
  auto *state = found->second;
  if (state->motion.target == geometry)
    return;
  state->motion.retarget(geometry);
  applyLive(state, monotonicNow());
}

void SceneAnimation::advance() { advance(monotonicNow()); }

void SceneAnimation::advance(std::uint64_t presentNs) {
  // Follow output presentation cadence; no independent timer redraws idle
  // frames. Several outputs may advance with their own present times: each
  // animation only moves forward in time.
  std::vector<LiveState *> live;
  live.reserve(live_.size());
  for (const auto &[tree, state] : live_) {
    (void)tree;
    live.push_back(state);
  }
  for (auto *state : live) {
    state->motion.step(std::min(maximumStepSeconds,
                                secondsBetween(state->lastStep, presentNs)));
    state->lastStep = std::max(state->lastStep, presentNs);
    if (state->motion.settled() &&
        timeProgress(state->started, presentNs, state->fadeDuration) >= 1.0) {
      auto *tree = state->tree;
      const bool hide = state->hiding;
      finishLive(tree, state);
      if (hide)
        wlr_scene_node_set_enabled(&tree->node, false);
    } else
      applyLive(state, presentNs);
  }

  const auto snapshots = snapshots_;
  for (auto *state : snapshots) {
    if (state->live)
      continue;
    if (state->moving) {
      state->motion.step(std::min(maximumStepSeconds,
                                  secondsBetween(state->lastStep, presentNs)));
      state->lastStep = std::max(state->lastStep, presentNs);
      if (state->motion.settled()) {
        destroySnapshot(state);
        continue;
      }
      applySnapshot(state, state->motion.current(),
                    state->fade ? 1.0 - state->motion.progress() : 1.0);
      continue;
    }
    const double progress =
        timeProgress(state->started, presentNs, closeDuration_);
    if (progress >= 1.0) {
      destroySnapshot(state);
      continue;
    }
    const double eased = progress * progress * progress;
    const auto &from = state->original;
    auto target = scaledAroundCenter(from, exitScale_);
    target.top += closeDrop;
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
