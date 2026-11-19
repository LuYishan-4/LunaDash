#include "compositor/renderer/blur/WindowGlass.hpp"
#include "compositor/renderer/blur/SceneBackdrop.h"
#include "compositor/renderer/clip/CornerGeometry.h"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "core/log/StallTrace.hpp"
#include "core/templates/LunaClint.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace LunaDash {
namespace {
using Clock = std::chrono::steady_clock;

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  bool intersects(const Rect &other) const {
    return width > 0 && height > 0 && other.width > 0 && other.height > 0 &&
           x < other.x + other.width && other.x < x + width &&
           y < other.y + other.height && other.y < y + height;
  }
  bool operator==(const Rect &) const = default;
};

struct Stopwatch {
  Clock::time_point started{};
  bool running = false;

  void restart() {
    started = Clock::now();
    running = true;
  }
  bool isValid() const { return running; }
  std::int64_t elapsed() const {
    if (!running)
      return 0;
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                                 started)
        .count();
  }
  void invalidate() { running = false; }
};

struct Fingerprint {
  std::uint64_t value = 14695981039346656037ULL;

  template <typename T> void add(const T &item) {
    const auto *bytes = reinterpret_cast<const unsigned char *>(&item);
    for (std::size_t i = 0; i < sizeof(item); ++i) {
      value ^= bytes[i];
      value *= 1099511628211ULL;
    }
  }
};

void setWindowOpacity(wlr_scene_buffer *buffer, int, int, void *data) {
  const float opacity = *static_cast<float *>(data);
  if (buffer->opacity != opacity)
    wlr_scene_buffer_set_opacity(buffer, opacity);
}
} // namespace

class WindowGlass::Impl : public Templates::LunaClint<WindowGlass::Impl> {
public:
  struct SurfaceWatch : Templates::LunaClint<SurfaceWatch> {
    Impl *owner = nullptr;
    wlr_surface *surface = nullptr;
    std::uint64_t revision = 0;
    bool rapid = false;
    Stopwatch commitClock;
    Templates::WaylandSlot<SurfaceWatch> commit;
    Templates::WaylandSlot<SurfaceWatch> destroy;

    SurfaceWatch(Impl *ownerValue, wlr_surface *value)
        : owner(ownerValue), surface(value) {
      listen<&SurfaceWatch::onCommit>(&surface->events.commit, commit);
      listen<&SurfaceWatch::onDestroy>(&surface->events.destroy, destroy);
    }

    void onCommit() {
      constexpr uint32_t content =
          WLR_SURFACE_STATE_BUFFER | WLR_SURFACE_STATE_SURFACE_DAMAGE |
          WLR_SURFACE_STATE_BUFFER_DAMAGE | WLR_SURFACE_STATE_TRANSFORM |
          WLR_SURFACE_STATE_SCALE | WLR_SURFACE_STATE_VIEWPORT |
          WLR_SURFACE_STATE_OFFSET;
      // Callback-only commits do not change pixels. Counting them would
      // let our own scene damage sustain an idle frame/blur loop.
      if (!(surface->current.committed & content))
        return;
      rapid = commitClock.isValid() && commitClock.elapsed() < 40;
      commitClock.restart();
      ++revision;
      ++owner->sceneRevision;
    }

    void onDestroy() {
      Templates::detachListeners(commit, destroy);
      surface = nullptr;
    }
  };

  struct Entry : Templates::LunaClint<Entry> {
    wlr_scene_tree *tree = nullptr;
    wlr_scene_tree *glass = nullptr;
    std::vector<wlr_scene_buffer *> parts;
    // Each part's full band within the glass (parts may be trimmed to an output).
    std::vector<ludash_corner_band> bands;
    wlr_buffer *backing = nullptr;
    Templates::WaylandSlot<Entry> treeDestroy;
    Templates::WaylandSlot<Entry> glassDestroy;
    Rect area;
    WindowGlass::Size clipSize;
    int cornerRadius = -1;
    std::uint64_t fingerprint = 0;
    std::uint64_t revision = 0;
    std::uint64_t sceneRevision = 0;
    bool attempted = false;
    bool rapidBackdrop = false;
    Stopwatch captureClock;
    std::string error;
    // The shared backdrop shown, by its revision (see setSharedBuffer).
    std::uint64_t sharedRevision = 0;
    bool shared = false;

    explicit Entry(wlr_scene_tree *value) : tree(value) {
      listen<&Entry::onTreeDestroy>(&tree->node.events.destroy, treeDestroy);
    }
    ~Entry() {
      Templates::detachListener(treeDestroy);
      clearGlass();
    }
    void onTreeDestroy() {
      Templates::detachListener(treeDestroy);
      tree = nullptr;
      clearGlass();
    }
    void onGlassDestroy() {
      Templates::detachListener(glassDestroy);
      glass = nullptr;
      parts.clear();
      clipSize = {};
      cornerRadius = -1;
      if (backing)
        wlr_buffer_unlock(backing);
      backing = nullptr;
      attempted = false;
      sceneRevision = 0;
      rapidBackdrop = false;
      captureClock.invalidate();
      sharedRevision = 0;
    }
    void clearGlass() {
      Templates::detachListener(glassDestroy);
      if (glass)
        wlr_scene_node_destroy(&glass->node);
      glass = nullptr;
      parts.clear();
      clipSize = {};
      cornerRadius = -1;
      if (backing)
        wlr_buffer_unlock(backing);
      backing = nullptr;
      attempted = false;
      sceneRevision = 0;
      rapidBackdrop = false;
      captureClock.invalidate();
      sharedRevision = 0;
    }
    bool createGlass() {
      if (glass)
        return true;
      glass = wlr_scene_tree_create(tree->node.parent);
      if (!glass)
        return false;
      wlr_scene_node_set_enabled(&glass->node, false);
      listen<&Entry::onGlassDestroy>(&glass->node.events.destroy, glassDestroy);
      return true;
    }

    bool configureClips(WindowGlass::Size size, int radius) {
      radius =
          std::max(0, std::min({radius, 32, size.width / 2, size.height / 2}));
      if (!parts.empty() && clipSize == size && cornerRadius == radius)
        return true;
      ludash_corner_band bands[LUDASH_CORNER_MAX_BANDS];
      const auto count = ludash_corner_bands(size.width, size.height, radius,
                                             bands, LUDASH_CORNER_MAX_BANDS);
      if (!count)
        return false;
      std::vector<wlr_scene_buffer *> next;
      std::vector<ludash_corner_band> nextBands(bands, bands + count);
      for (std::size_t i = 0; i < count; ++i) {
        auto *part = wlr_scene_buffer_create(glass, nullptr);
        if (!part) {
          for (auto *created : next)
            wlr_scene_node_destroy(&created->node);
          return false;
        }
        part->point_accepts_input = [](wlr_scene_buffer *, double *, double *) {
          return false;
        };
        const auto &band = bands[i];
        wlr_scene_node_set_position(&part->node, band.x, band.y);
        wlr_scene_buffer_set_dest_size(part, band.width, band.height);
        next.push_back(part);
      }
      for (auto *part : parts)
        wlr_scene_node_destroy(&part->node);
      parts = std::move(next);
      this->bands = std::move(nextBands);
      clipSize = size;
      cornerRadius = radius;
      attempted = false;
      sharedRevision = 0;
      return true;
    }

    void setBuffer(wlr_buffer *buffer) {
      // A capture covers the whole glass: every part shows its full band.
      for (std::size_t i = 0; i < parts.size() && i < bands.size(); ++i) {
        wlr_scene_node_set_position(&parts[i]->node, bands[i].x, bands[i].y);
        wlr_scene_buffer_set_dest_size(parts[i], bands[i].width, bands[i].height);
        if (!parts[i]->node.enabled)
          wlr_scene_node_set_enabled(&parts[i]->node, true);
      }
      for (auto *part : parts) {
        const wlr_fbox crop{
            double(part->node.x) * buffer->width / clipSize.width,
            double(part->node.y) * buffer->height / clipSize.height,
            double(part->dst_width) * buffer->width / clipSize.width,
            double(part->dst_height) * buffer->height / clipSize.height};
        wlr_scene_buffer_set_source_box(part, &crop);
        wlr_scene_buffer_set_buffer(part, buffer);
      }
    }

    // Shows the part of an output's backdrop (covering `box`) that lies under
    // the glass at scene position x, y. Only changes reach the scene: an
    // unchanged crop or buffer adds no damage.
    void setSharedBuffer(wlr_buffer *buffer, std::uint64_t revision,
                         const Rect &box, int x, int y) {
      const bool replaced = sharedRevision != revision;
      sharedRevision = revision;
      const double scaleX = double(buffer->width) / box.width;
      const double scaleY = double(buffer->height) / box.height;
      for (std::size_t i = 0; i < parts.size() && i < bands.size(); ++i) {
        auto *part = parts[i];
        const auto &band = bands[i];
        // The part of the band on this output, in scene coordinates. A band
        // reaching past the output is trimmed, never stretched: stretching
        // changes with every pixel the window moves and makes the glass swim.
        const int left = std::max(x + band.x, box.x);
        const int top = std::max(y + band.y, box.y);
        const int right = std::min(x + band.x + band.width, box.x + box.width);
        const int bottom = std::min(y + band.y + band.height, box.y + box.height);
        const bool visible = right > left && bottom > top;
        // Parts off the output get the new backdrop too: they show it as soon
        // as they come back on screen.
        if (replaced)
          wlr_scene_buffer_set_buffer(part, buffer);
        if (part->node.enabled != visible)
          wlr_scene_node_set_enabled(&part->node, visible);
        if (!visible)
          continue;
        wlr_scene_node_set_position(&part->node, left - x, top - y);
        wlr_scene_buffer_set_dest_size(part, right - left, bottom - top);
        const wlr_fbox crop{(left - box.x) * scaleX, (top - box.y) * scaleY,
                            (right - left) * scaleX, (bottom - top) * scaleY};
        wlr_scene_buffer_set_source_box(part, &crop);
      }
    }
  };

  // The blurred content below the window layer on one output.
  struct Backdrop {
    Rect box;
    wlr_buffer *buffer = nullptr;
    std::uint64_t fingerprint = 0;
    std::uint64_t revision = 0;
    bool attempted = false;
    Stopwatch clock;

    Backdrop() = default;
    Backdrop(const Backdrop &) = delete;
    Backdrop &operator=(const Backdrop &) = delete;
    ~Backdrop() {
      if (buffer)
        wlr_buffer_unlock(buffer);
    }
  };

  wlr_renderer *renderer;
  wlr_allocator *allocator;
  // Where held explicit-sync client buffers are released (SceneBackdrop).
  wl_event_loop *loop = nullptr;
  wlr_scene_node *root;
  // Shared backdrops capture everything painted before this node.
  wlr_scene_node *windowLayer = nullptr;
  std::vector<std::unique_ptr<Backdrop>> backdrops;
  std::uint64_t backdropRevision = 0;
  // A capture put off by throttling asks for a frame when it is due: frames
  // stop once nothing moves, and the glass would keep the stale capture.
  wl_event_source *timer = nullptr;
  Clock::time_point timerDue{};
  bool timerArmed = false;
  std::function<void()> requestFrame;
  Templates::WaylandSlot<Impl> rootDestroy;
  std::unordered_map<wlr_scene_tree *, std::unique_ptr<Entry>> entries;
  std::unordered_map<wlr_surface *, std::unique_ptr<SurfaceWatch>> watches;
  bool enabled = true;
  std::uint64_t sceneRevision = 1;
  int radius = 18;
  float opacity = 0.96f;
  bool ready = false;
  bool failed = false;
  std::uint64_t frames = 0;
  std::string error;

  Impl(wlr_renderer *rendererValue, wlr_allocator *allocatorValue,
       wlr_scene_node *rootValue)
      : renderer(rendererValue), allocator(allocatorValue), root(rootValue) {
    if (root)
      listen<&Impl::onRootDestroy>(&root->events.destroy, rootDestroy);
  }
  void onRootDestroy() {
    Templates::detachListener(rootDestroy);
    entries.clear();
    backdrops.clear();
    watches.clear();
    root = nullptr;
    windowLayer = nullptr;
  }
  ~Impl() {
    Templates::detachListener(rootDestroy);
    if (timer)
      wl_event_source_remove(timer);
    entries.clear();
    backdrops.clear();
    watches.clear();
    ludash_scene_backdrop_finish();
  }

  static int onTimer(void *data) {
    auto *self = static_cast<Impl *>(data);
    self->timerArmed = false;
    if (self->requestFrame)
      self->requestFrame();
    return 0;
  }

  // Asks for a frame in `milliseconds` (the earliest of pending requests).
  void defer(std::int64_t milliseconds) {
    if (!loop)
      return;
    milliseconds = std::max<std::int64_t>(1, milliseconds);
    const auto due = Clock::now() + std::chrono::milliseconds(milliseconds);
    if (timerArmed && timerDue <= due)
      return;
    if (!timer)
      timer = wl_event_loop_add_timer(loop, &Impl::onTimer, this);
    if (!timer)
      return;
    timerDue = due;
    timerArmed = true;
    wl_event_source_timer_update(timer, static_cast<int>(milliseconds));
  }

  SurfaceWatch *surfaceWatch(wlr_surface *surface) {
    auto &watch = watches[surface];
    if (!watch || watch->surface != surface)
      watch = std::make_unique<SurfaceWatch>(this, surface);
    return watch.get();
  }

  // Return true at `stop` (the target window): only earlier painter-order
  // nodes belong in its backdrop. `skip` (its own underlay) is left out.
  bool fingerprint(wlr_scene_node *node, wlr_scene_node *stop,
                   wlr_scene_node *skip, Fingerprint &hash, const Rect &sample,
                   bool &rapidSource, int x = 0, int y = 0, int depth = 0) {
    if (node == stop)
      return true;
    if (!node || !node->enabled || node == skip || depth > 128)
      return false;
    x += node->x;
    y += node->y;
    if (node->type == WLR_SCENE_NODE_TREE) {
      auto *tree = wlr_scene_tree_from_node(node);
      wlr_scene_node *child = nullptr;
      wl_list_for_each(child, &tree->children,
                       link) if (fingerprint(child, stop, skip, hash, sample,
                                             rapidSource, x, y,
                                             depth + 1)) return true;
    } else if (node->type == WLR_SCENE_NODE_RECT) {
      const auto *rect = wlr_scene_rect_from_node(node);
      if (!sample.intersects(Rect{x, y, rect->width, rect->height}))
        return false;
      hash.add(node);
      hash.add(x);
      hash.add(y);
      hash.add(rect->width);
      hash.add(rect->height);
      for (const auto color : rect->color)
        hash.add(color);
    } else if (node->type == WLR_SCENE_NODE_BUFFER) {
      auto *buffer = wlr_scene_buffer_from_node(node);
      if (buffer->opacity <= 0)
        return false;
      // Match capture's spatial bounds. Updating an unrelated tiled window or
      // panel must not invalidate every later window's backdrop. Do not hash
      // empty containers: only contributing leaves and painter order matter.
      if (buffer->dst_width > 0 && buffer->dst_height > 0 &&
          !sample.intersects(Rect{x, y, buffer->dst_width, buffer->dst_height}))
        return false;
      // Slivers (a shell's 1 px exclusive-zone reservers) vanish in the blur:
      // their commits must not capture everything again.
      if ((buffer->dst_width > 0 && buffer->dst_width <= 2) ||
          (buffer->dst_height > 0 && buffer->dst_height <= 2))
        return false;
      hash.add(node);
      hash.add(x);
      hash.add(y);
      hash.add(buffer->opacity);
      hash.add(buffer->dst_width);
      hash.add(buffer->dst_height);
      hash.add(buffer->src_box.x);
      hash.add(buffer->src_box.y);
      hash.add(buffer->src_box.width);
      hash.add(buffer->src_box.height);
      hash.add(buffer->transform);
      if (auto *surface = wlr_scene_surface_try_from_buffer(buffer)) {
        hash.add(surface->surface->buffer);
        auto *watch = surfaceWatch(surface->surface);
        hash.add(watch->revision);
        rapidSource = rapidSource || watch->rapid;
      } else {
        // Keep lower glass stable after scene texture upload releases its
        // buffer pointer. A successful lower capture increments its revision.
        bool managed = false;
        for (const auto &[tree, candidate] : entries) {
          (void)tree;
          if (candidate->glass && buffer->node.parent == candidate->glass) {
            hash.add(candidate->revision);
            managed = true;
            break;
          }
        }
        if (!managed)
          hash.add(buffer->buffer);
      }
    }
    return false;
  }

  void collect(wlr_scene_node *node, std::vector<Entry *> &ordered,
               int depth = 0) {
    if (!node || !node->enabled || node->type != WLR_SCENE_NODE_TREE ||
        depth > 128)
      return;
    auto *tree = wlr_scene_tree_from_node(node);
    const auto found = entries.find(tree);
    if (found != entries.end() && found->second->tree)
      ordered.push_back(found->second.get());
    wlr_scene_node *child = nullptr;
    wl_list_for_each(child, &tree->children, link)
        collect(child, ordered, depth + 1);
  }

  void capture(Entry &entry, bool animationsActive) {
    const std::int64_t rapidIntervalMs = entries.size() >= 4 ? 100 : 66;
    constexpr std::int64_t staticValidationMs = 100;
    const bool available = entry.backing && entry.error.empty();
    if (available && entry.attempted && !animationsActive &&
        !entry.rapidBackdrop && entry.sceneRevision == sceneRevision &&
        entry.captureClock.isValid() &&
        entry.captureClock.elapsed() < staticValidationMs) {
      if (!entry.glass->node.enabled)
        wlr_scene_node_set_enabled(&entry.glass->node, true);
      ready = true;
      return;
    }
    if (available && entry.attempted &&
        (animationsActive ||
         (entry.rapidBackdrop && entry.captureClock.isValid() &&
          entry.captureClock.elapsed() < rapidIntervalMs))) {
      if (!animationsActive)
        defer(rapidIntervalMs - entry.captureClock.elapsed());
      if (!entry.glass->node.enabled)
        wlr_scene_node_set_enabled(&entry.glass->node, true);
      ready = true;
      return;
    }

    Fingerprint hash;
    hash.add(entry.area.x);
    hash.add(entry.area.y);
    hash.add(entry.area.width);
    hash.add(entry.area.height);
    hash.add(radius);
    hash.add(entry.cornerRadius);
    const wlr_box area{entry.area.x, entry.area.y, entry.area.width,
                       entry.area.height};
    wlr_box sample{};
    ludash_scene_backdrop_sample_box(&area, radius, &sample);
    bool rapidSource = false;
    const bool found = fingerprint(
        root, &entry.tree->node, &entry.glass->node, hash,
        Rect{sample.x, sample.y, sample.width, sample.height}, rapidSource);
    if (!found)
      return;
    entry.sceneRevision = sceneRevision;
    if (available && rapidSource && entry.captureClock.isValid() &&
        entry.captureClock.elapsed() < rapidIntervalMs) {
      entry.rapidBackdrop = true;
      defer(rapidIntervalMs - entry.captureClock.elapsed());
      if (!entry.glass->node.enabled)
        wlr_scene_node_set_enabled(&entry.glass->node, true);
      ready = true;
      return;
    }
    if (!entry.attempted || entry.fingerprint != hash.value) {
      entry.attempted = true;
      entry.fingerprint = hash.value;
      StallTrace::Section trace("glass capture", "surface");
      auto *buffer = ludash_scene_backdrop_render(
          renderer, allocator, loop, root, &entry.tree->node, &entry.glass->node,
          &area, radius);
      if (buffer) {
        entry.setBuffer(buffer);
        if (entry.backing)
          wlr_buffer_unlock(entry.backing);
        // Hold an extra consumer lock after scene texture upload so higher
        // windows can still import this underlay through its scene buffer.
        entry.backing = wlr_buffer_lock(buffer);
        wlr_buffer_drop(buffer);
        ++entry.revision;
        ++frames;
        entry.error.clear();
        entry.rapidBackdrop = rapidSource;
        entry.captureClock.restart();
      } else {
        entry.error = "The renderer could not create a window backdrop.";
      }
    } else {
      // Nothing changed. Bound the next full scene walk instead of repeating
      // the same fingerprint on every high-refresh output frame.
      entry.rapidBackdrop = rapidSource;
      entry.captureClock.restart();
    }
    const bool nowAvailable = entry.backing && entry.error.empty();
    if (entry.glass->node.enabled != nowAvailable)
      wlr_scene_node_set_enabled(&entry.glass->node, nowAvailable);
    ready = ready || nowAvailable;
    if (!entry.error.empty()) {
      failed = true;
      error = entry.error;
    }
  }

  // What made the last backdrop capture: the layer surfaces (by namespace) that
  // drew something new since the capture before it (lunadashctl status, blurCause).
  std::unordered_map<wlr_surface *, std::uint64_t> capturedRevisions;
  std::string cause;

  void noteCause() {
    std::string changed;
    for (const auto &[surface, watch] : watches) {
      if (!watch->surface)
        continue;
      auto &seen = capturedRevisions[surface];
      if (seen == watch->revision)
        continue;
      seen = watch->revision;
      auto *layer = wlr_layer_surface_v1_try_from_wlr_surface(surface);
      const std::string name =
          layer && layer->namespace_ ? layer->namespace_ : "surface";
      if (changed.find(name) == std::string::npos)
        changed += (changed.empty() ? "" : ", ") + name;
    }
    std::erase_if(capturedRevisions,
                  [this](const auto &item) { return !watches.contains(item.first); });
    cause = changed.empty() ? "layout" : changed;
  }

  // Captures again what lies below the window layer on this output when it
  // changed, at most every 33 ms (a wallpaper crossfade, say).
  void refresh(Backdrop &backdrop) {
    constexpr std::int64_t intervalMs = 33;
    Fingerprint hash;
    hash.add(backdrop.box.x);
    hash.add(backdrop.box.y);
    hash.add(backdrop.box.width);
    hash.add(backdrop.box.height);
    hash.add(radius);
    bool rapidSource = false;
    if (!fingerprint(root, windowLayer, nullptr, hash, backdrop.box,
                     rapidSource))
      return;
    if (backdrop.attempted && backdrop.fingerprint == hash.value)
      return;
    if (backdrop.buffer && backdrop.clock.isValid() &&
        backdrop.clock.elapsed() < intervalMs) {
      defer(intervalMs - backdrop.clock.elapsed());
      return;
    }
    backdrop.attempted = true;
    backdrop.fingerprint = hash.value;
    backdrop.clock.restart();
    const wlr_box area{backdrop.box.x, backdrop.box.y, backdrop.box.width,
                       backdrop.box.height};
    StallTrace::Section trace("glass capture", "backdrop");
    auto *buffer = ludash_scene_backdrop_render(
        renderer, allocator, loop, root, windowLayer, nullptr, &area, radius);
    if (!buffer) {
      failed = true;
      error = "The renderer could not create the window backdrop.";
      return;
    }
    if (backdrop.buffer)
      wlr_buffer_unlock(backdrop.buffer);
    backdrop.buffer = wlr_buffer_lock(buffer);
    wlr_buffer_drop(buffer);
    backdrop.revision = ++backdropRevision;
    ++frames;
    noteCause();
  }

  // The output under the middle of `area`, else the first one.
  Backdrop *backdropFor(const Rect &area) {
    const int x = area.x + area.width / 2;
    const int y = area.y + area.height / 2;
    for (auto &backdrop : backdrops)
      if (x >= backdrop->box.x && x < backdrop->box.x + backdrop->box.width &&
          y >= backdrop->box.y && y < backdrop->box.y + backdrop->box.height)
        return backdrop.get();
    return backdrops.empty() ? nullptr : backdrops.front().get();
  }
};

WindowGlass::WindowGlass(wlr_renderer *renderer, wlr_allocator *allocator,
                         wlr_scene_node *root, wlr_scene_node *windowLayer,
                         wl_event_loop *loop)
    : d(std::make_unique<Impl>(renderer, allocator, root)) {
  d->loop = loop;
  d->windowLayer = windowLayer;
}

void WindowGlass::setFrameRequest(std::function<void()> request) {
  d->requestFrame = std::move(request);
}
WindowGlass::~WindowGlass() = default;

bool WindowGlass::configure(bool blurEnabled, int radius, float opacity) {
  radius = std::clamp(radius, 0, 32);
  opacity = std::clamp(opacity, 0.0f, 1.0f);
  const bool changed =
      d->enabled != blurEnabled || d->radius != radius || d->opacity != opacity;
  d->enabled = blurEnabled;
  d->radius = radius;
  d->opacity = opacity;
  return changed;
}

void WindowGlass::update(const std::vector<Surface> &surfaces,
                         const std::vector<Output> &outputs,
                         bool animationsActive) {
  d->ready = false;
  d->failed = false;
  d->error.clear();
  if (!d->root)
    return;
  std::unordered_set<wlr_scene_tree *> active;
  for (const auto &surface : surfaces) {
    auto *tree = surface.tree;
    if (!tree)
      continue;
    // A fade owns the alpha of its window. Otherwise every buffer of the
    // window gets the window opacity on every frame, during slides and moves
    // too: rounded corners recreate their views and clients add subsurfaces at
    // any time, and a value applied once would leave those new buffers
    // opaque. An unchanged value costs nothing.
    if (!surface.fading) {
      float value = surface.enabled
                        ? (surface.opacity >= 0.0f
                               ? std::clamp(surface.opacity, 0.0f, 1.0f)
                               : d->opacity)
                        : 1.0f;
      wlr_scene_node_for_each_buffer(&tree->node, setWindowOpacity, &value);
    }
    int x = 0, y = 0;
    if (!d->enabled || d->radius == 0 || !surface.enabled ||
        !surface.size.valid() || !tree->node.parent ||
        !wlr_scene_node_coords(&tree->node, &x, &y))
      continue;
    active.insert(tree);
    auto &entry = d->entries[tree];
    if (!entry || entry->tree != tree)
      entry = std::make_unique<Impl::Entry>(tree);
    if (!entry->createGlass() ||
        !entry->configureClips(surface.size, surface.cornerRadius)) {
      entry->clearGlass();
      d->failed = true;
      d->error = "The renderer could not allocate a window backdrop node.";
      continue;
    }
    bool sceneChanged = false;
    if (entry->glass->node.parent != tree->node.parent) {
      wlr_scene_node_reparent(&entry->glass->node, tree->node.parent);
      sceneChanged = true;
    }
    if (tree->node.link.prev != &entry->glass->node.link) {
      wlr_scene_node_place_below(&entry->glass->node, &tree->node);
      sceneChanged = true;
    }
    if (entry->glass->node.x != tree->node.x ||
        entry->glass->node.y != tree->node.y) {
      wlr_scene_node_set_position(&entry->glass->node, tree->node.x,
                                  tree->node.y);
      sceneChanged = true;
    }
    const Rect nextArea{x, y, surface.size.width, surface.size.height};
    if (entry->shared != surface.shared) {
      entry->shared = surface.shared;
      entry->attempted = false;
      entry->sharedRevision = 0;
    }
    if (entry->area != nextArea) {
      entry->area = nextArea;
      entry->attempted = false;
      // A moving shared glass only crops differently: nothing to capture.
      sceneChanged = sceneChanged || !entry->shared;
    }
    if (sceneChanged)
      ++d->sceneRevision;
  }
  std::erase_if(d->entries, [&active](const auto &item) {
    return !active.contains(item.first) || !item.second->tree;
  });
  std::erase_if(d->watches,
                [](const auto &item) { return !item.second->surface; });
  if (d->entries.empty()) {
    d->watches.clear();
    d->backdrops.clear();
    return;
  }

  // One backdrop per output, for every shared glass on it.
  const bool anyShared =
      d->windowLayer &&
      std::any_of(d->entries.begin(), d->entries.end(),
                  [](const auto &item) { return item.second->shared; });
  if (!anyShared)
    d->backdrops.clear();
  else {
    d->backdrops.resize(outputs.size());
    for (std::size_t i = 0; i < outputs.size(); ++i) {
      auto &backdrop = d->backdrops[i];
      const Rect box{outputs[i].x, outputs[i].y, outputs[i].width,
                     outputs[i].height};
      if (!backdrop || backdrop->box != box) {
        backdrop = std::make_unique<Impl::Backdrop>();
        backdrop->box = box;
      }
      if (box.width > 0 && box.height > 0)
        d->refresh(*backdrop);
    }
  }

  std::vector<Impl::Entry *> ordered;
  d->collect(d->root, ordered);
  for (auto *entry : ordered) {
    if (!entry->glass)
      continue;
    if (!entry->shared) {
      d->capture(*entry, animationsActive);
      continue;
    }
    auto *backdrop = d->backdropFor(entry->area);
    const bool available = backdrop && backdrop->buffer;
    if (available)
      entry->setSharedBuffer(backdrop->buffer, backdrop->revision,
                             backdrop->box, entry->area.x, entry->area.y);
    if (entry->glass->node.enabled != available)
      wlr_scene_node_set_enabled(&entry->glass->node, available);
    d->ready = d->ready || available;
  }
}

bool WindowGlass::ready() const { return d->ready; }
bool WindowGlass::failed() const { return d->failed; }
std::uint64_t WindowGlass::frames() const { return d->frames; }
std::string WindowGlass::error() const { return d->error; }
std::string WindowGlass::cause() const { return d->cause; }

} // namespace LunaDash
