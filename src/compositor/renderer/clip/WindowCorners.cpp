#include "compositor/renderer/clip/WindowCorners.hpp"
#include "compositor/renderer/clip/CornerGeometry.h"

// C++ library headers must be parsed before the wlroots compatibility macros.
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "core/templates/LunaClint.hpp"

#if __has_include(<wlr/render/drm_syncobj.h>)
extern "C" {
#include <wlr/render/drm_syncobj.h>
}
#define LUDASH_CORNERS_SYNCOBJ 1
#else
#define LUDASH_CORNERS_SYNCOBJ 0
#endif


namespace LunaDash {
namespace {
void initialOpacity(wlr_scene_buffer *buffer, int, int, void *data) {
  wlr_scene_buffer_set_opacity(buffer, *static_cast<float *>(data));
}

#if LUDASH_CORNERS_SYNCOBJ
// Timeline points known to be signaled during one settle pass.
struct SettledPoints {
  std::vector<std::pair<wlr_drm_syncobj_timeline *, std::uint64_t>> signaled;
  std::vector<std::pair<wlr_drm_syncobj_timeline *, std::uint64_t>> pending;
};

// A band whose client buffer is already drawn needs no GPU wait: drop it. A
// point not yet signaled keeps its wait, so nothing is read too early.
void settleWait(wlr_scene_buffer *buffer, int, int, void *data) {
  auto &state = buffer->WLR_PRIVATE;
  if (!state.wait_timeline)
    return;
  auto &points = *static_cast<SettledPoints *>(data);
  const std::pair key{state.wait_timeline, state.wait_point};
  bool signaled = std::find(points.signaled.begin(), points.signaled.end(), key) !=
                  points.signaled.end();
  if (!signaled) {
    if (std::find(points.pending.begin(), points.pending.end(), key) !=
        points.pending.end())
      return;
    bool result = false;
    signaled = wlr_drm_syncobj_timeline_check(state.wait_timeline,
                                              state.wait_point, 0, &result) &&
               result;
    (signaled ? points.signaled : points.pending).push_back(key);
  }
  if (!signaled)
    return;
  wlr_drm_syncobj_timeline_unref(state.wait_timeline);
  state.wait_timeline = nullptr;
  state.wait_point = 0;
}
#endif
} // namespace

class WindowCorners::Impl {
public:
  struct Entry : Templates::LunaClint<Entry> {
    wlr_scene_tree *tree;
    wlr_scene_tree *content;
    wlr_surface *surface;
    wlr_scene_tree *views = nullptr;
    WindowCorners::Size size;
    WindowCorners::Point origin;
    int radius = 0;
    Templates::WaylandSlot treeDestroy;
    Templates::WaylandSlot contentDestroy;
    Templates::WaylandSlot surfaceDestroy;
    Templates::WaylandSlot viewsDestroy;

    explicit Entry(const Surface &value)
        : tree(value.tree), content(value.content), surface(value.surface) {
      listen<&Entry::onTreeDestroy, &Entry::treeDestroy>(
          &tree->node.events.destroy);
      listen<&Entry::onContentDestroy, &Entry::contentDestroy>(
          &content->node.events.destroy);
      listen<&Entry::onSurfaceDestroy, &Entry::surfaceDestroy>(
          &surface->events.destroy);
    }
    ~Entry() {
      Templates::detachListeners(treeDestroy, contentDestroy, surfaceDestroy);
      clearViews(true);
    }

    void onTreeDestroy() {
      Templates::detachListener(treeDestroy);
      tree = nullptr;
      clearViews(false);
    }
    void onContentDestroy() {
      Templates::detachListener(contentDestroy);
      content = nullptr;
      clearViews(false);
    }
    void onSurfaceDestroy() {
      Templates::detachListener(surfaceDestroy);
      surface = nullptr;
      clearViews(false);
    }
    void onViewsDestroy() {
      Templates::detachListener(viewsDestroy);
      views = nullptr;
      radius = 0;
    }

    void clearViews(bool restore) {
      Templates::detachListener(viewsDestroy);
      if (views)
        wlr_scene_node_destroy(&views->node);
      views = nullptr;
      radius = 0;
      if (restore && tree && content)
        wlr_scene_node_set_enabled(&content->node, true);
    }

    void configure(const Surface &value) {
      const int requested = std::max(
          0, std::min({16, value.size.width / 2, value.size.height / 2}));
      if (views && size == value.size && origin == value.surfaceOrigin &&
          radius == requested) {
        // The original xdg helper remains responsible for protocol state.
        // Its content is rendered through the clipped views instead.
        if (content->node.enabled)
          wlr_scene_node_set_enabled(&content->node, false);
        return;
      }
      ludash_corner_band bands[LUDASH_CORNER_MAX_BANDS];
      const auto count =
          ludash_corner_bands(value.size.width, value.size.height, requested,
                              bands, LUDASH_CORNER_MAX_BANDS);
      if (!count) {
        clearViews(true);
        return;
      }
      auto *next = wlr_scene_tree_create(tree);
      if (!next) {
        clearViews(true);
        return;
      }
      wlr_scene_node_set_enabled(&next->node, false);
      bool complete = true;
      for (std::size_t i = 0; i < count; ++i) {
        auto *view = wlr_scene_subsurface_tree_create(next, surface);
        if (!view) {
          complete = false;
          break;
        }
        wlr_scene_node_set_position(&view->node, -value.surfaceOrigin.x,
                                    -value.surfaceOrigin.y);
        const auto &band = bands[i];
        wlr_box clip{band.x + value.surfaceOrigin.x,
                     band.y + value.surfaceOrigin.y, band.width, band.height};
        wlr_scene_subsurface_tree_set_clip(&view->node, &clip);
        // Traverse the enabled view itself: its container stays hidden until
        // all bands are ready, and traversal does not consult ancestors.
        float opacity = std::clamp(value.opacity, 0.0f, 1.0f);
        wlr_scene_node_for_each_buffer(&view->node, initialOpacity, &opacity);
      }
      if (!complete) {
        wlr_scene_node_destroy(&next->node);
        clearViews(true);
        return;
      }
      clearViews(false);
      views = next;
      listen<&Entry::onViewsDestroy, &Entry::viewsDestroy>(
          &views->node.events.destroy);
      wlr_scene_node_place_below(&views->node, &content->node);
      wlr_scene_node_set_enabled(&views->node, true);
      wlr_scene_node_set_enabled(&content->node, false);
      size = value.size;
      origin = value.surfaceOrigin;
      radius = requested;
    }
  };

  std::unordered_map<wlr_scene_tree *, std::unique_ptr<Entry>> entries;
};

WindowCorners::WindowCorners() : d(std::make_unique<Impl>()) {}
WindowCorners::~WindowCorners() = default;

void WindowCorners::update(const std::vector<Surface> &surfaces) {
  std::unordered_set<wlr_scene_tree *> active;
  for (const auto &surface : surfaces) {
    if (!surface.enabled || !surface.tree || !surface.content ||
        !surface.surface || surface.content->node.parent != surface.tree ||
        surface.size.empty())
      continue;
    active.insert(surface.tree);
    auto &entry = d->entries[surface.tree];
    if (!entry || entry->tree != surface.tree ||
        entry->content != surface.content || entry->surface != surface.surface)
      entry = std::make_unique<Impl::Entry>(surface);
    entry->configure(surface);
  }
  std::erase_if(d->entries, [&active](const auto &item) {
    return !active.contains(item.first) || !item.second->tree ||
           !item.second->content || !item.second->surface;
  });
}

void WindowCorners::settleWaits() {
#if LUDASH_CORNERS_SYNCOBJ
  // Every band is a view of the same client buffer, and each view would make
  // the renderer wait on the client's acquire point on its own: a sync file
  // export and an EGL sync per band, per frame. Explicitly synchronized
  // clients (most GL/Vulkan ones on NVIDIA) then cost milliseconds a frame.
  SettledPoints points;
  for (const auto &[tree, entry] : d->entries)
    if (entry->views)
      wlr_scene_node_for_each_buffer(&entry->views->node, settleWait, &points);
#endif
}

int WindowCorners::radius(wlr_scene_tree *tree) const {
  const auto found = d->entries.find(tree);
  return found == d->entries.end() ? 0 : found->second->radius;
}

} // namespace LunaDash
