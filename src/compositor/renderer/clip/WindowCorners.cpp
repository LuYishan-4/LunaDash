#include "compositor/renderer/clip/WindowCorners.hpp"
#include "compositor/renderer/clip/CornerGeometry.h"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "core/templates/WaylandSlot.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace LunaDash {
namespace {
void initialOpacity(wlr_scene_buffer *buffer, int, int, void *data) {
  wlr_scene_buffer_set_opacity(buffer, *static_cast<float *>(data));
}
} // namespace

class WindowCorners::Impl {
public:
  struct Entry {
    wlr_scene_tree *tree;
    wlr_scene_tree *content;
    wlr_surface *surface;
    wlr_scene_tree *views = nullptr;
    QSize size;
    QPoint origin;
    int radius = 0;
    Templates::WaylandSlot<Entry> treeDestroy;
    Templates::WaylandSlot<Entry> contentDestroy;
    Templates::WaylandSlot<Entry> surfaceDestroy;
    Templates::WaylandSlot<Entry> viewsDestroy;

    explicit Entry(const Surface &value)
        : tree(value.tree), content(value.content), surface(value.surface) {
      Templates::attachListener(
          &tree->node.events.destroy, treeDestroy, this,
          [](wl_listener *listener, void *) {
            auto *entry = Templates::listenerOwner<Entry>(listener);
            Templates::detachListener(entry->treeDestroy);
            entry->tree = nullptr;
            entry->clearViews(false);
          });
      Templates::attachListener(
          &content->node.events.destroy, contentDestroy, this,
          [](wl_listener *listener, void *) {
            auto *entry = Templates::listenerOwner<Entry>(listener);
            Templates::detachListener(entry->contentDestroy);
            entry->content = nullptr;
            entry->clearViews(false);
          });
      Templates::attachListener(
          &surface->events.destroy, surfaceDestroy, this,
          [](wl_listener *listener, void *) {
            auto *entry = Templates::listenerOwner<Entry>(listener);
            Templates::detachListener(entry->surfaceDestroy);
            entry->surface = nullptr;
            entry->clearViews(false);
          });
    }
    ~Entry() {
      Templates::detachListener(treeDestroy);
      Templates::detachListener(contentDestroy);
      Templates::detachListener(surfaceDestroy);
      clearViews(true);
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
      const int requested = std::max(0, std::min({16, value.size.width() / 2,
                                                 value.size.height() / 2}));
      if (views && size == value.size && origin == value.surfaceOrigin &&
          radius == requested) {
        // The original xdg helper remains responsible for protocol state.
        // Its content is rendered through the clipped views instead.
        if (content->node.enabled)
          wlr_scene_node_set_enabled(&content->node, false);
        return;
      }
      ludash_corner_band bands[LUDASH_CORNER_MAX_BANDS];
      const auto count = ludash_corner_bands(value.size.width(), value.size.height(),
                                             requested, bands, LUDASH_CORNER_MAX_BANDS);
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
        wlr_scene_node_set_position(&view->node, -value.surfaceOrigin.x(),
                                    -value.surfaceOrigin.y());
        const auto &band = bands[i];
        wlr_box clip{band.x + value.surfaceOrigin.x(),
                            band.y + value.surfaceOrigin.y(), band.width, band.height};
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
      Templates::attachListener(
          &views->node.events.destroy, viewsDestroy, this,
          [](wl_listener *listener, void *) {
            auto *entry = Templates::listenerOwner<Entry>(listener);
            Templates::detachListener(entry->viewsDestroy);
            entry->views = nullptr;
            entry->radius = 0;
          });
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

void WindowCorners::update(const QList<Surface> &surfaces) {
  std::unordered_set<wlr_scene_tree *> active;
  for (const auto &surface : surfaces) {
    if (!surface.enabled || !surface.tree || !surface.content ||
        !surface.surface || surface.content->node.parent != surface.tree ||
        surface.size.isEmpty())
      continue;
    active.insert(surface.tree);
    auto &entry = d->entries[surface.tree];
    if (!entry || entry->tree != surface.tree || entry->content != surface.content ||
        entry->surface != surface.surface)
      entry = std::make_unique<Impl::Entry>(surface);
    entry->configure(surface);
  }
  std::erase_if(d->entries, [&active](const auto &item) {
    return !active.contains(item.first) || !item.second->tree ||
           !item.second->content || !item.second->surface;
  });
}

int WindowCorners::radius(wlr_scene_tree *tree) const {
  const auto found = d->entries.find(tree);
  return found == d->entries.end() ? 0 : found->second->radius;
}

} // namespace LunaDash
