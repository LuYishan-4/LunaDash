#include "compositor/renderer/blur/WindowGlass.hpp"
#include "compositor/renderer/blur/SceneBackdrop.h"
#include "compositor/renderer/clip/CornerGeometry.h"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "core/templates/WaylandSlot.hpp"

#include <QRect>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace LunaDash {
namespace {
struct Fingerprint {
  quint64 value = 14695981039346656037ULL;

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

class WindowGlass::Impl {
public:
  struct SurfaceWatch {
    wlr_surface *surface = nullptr;
    quint64 revision = 0;
    Templates::WaylandSlot<SurfaceWatch> commit;
    Templates::WaylandSlot<SurfaceWatch> destroy;

    explicit SurfaceWatch(wlr_surface *value) : surface(value) {
      Templates::attachListener(
          &surface->events.commit, commit, this,
          [](wl_listener *listener, void *) {
            auto *watch = Templates::listenerOwner<SurfaceWatch>(listener);
            constexpr uint32_t content = WLR_SURFACE_STATE_BUFFER |
                WLR_SURFACE_STATE_SURFACE_DAMAGE | WLR_SURFACE_STATE_BUFFER_DAMAGE |
                WLR_SURFACE_STATE_TRANSFORM | WLR_SURFACE_STATE_SCALE |
                WLR_SURFACE_STATE_VIEWPORT | WLR_SURFACE_STATE_OFFSET;
            // Callback-only commits do not change pixels. Counting them would
            // let our own scene damage sustain an idle frame/blur loop.
            if (watch->surface->current.committed & content)
              ++watch->revision;
          });
      Templates::attachListener(
          &surface->events.destroy, destroy, this,
          [](wl_listener *listener, void *) {
            auto *watch = Templates::listenerOwner<SurfaceWatch>(listener);
            Templates::detachListener(watch->commit);
            Templates::detachListener(watch->destroy);
            watch->surface = nullptr;
          });
    }
    ~SurfaceWatch() {
      Templates::detachListener(commit);
      Templates::detachListener(destroy);
    }
  };

  struct Entry {
    wlr_scene_tree *tree = nullptr;
    wlr_scene_tree *glass = nullptr;
    std::vector<wlr_scene_buffer *> parts;
    wlr_buffer *backing = nullptr;
    Templates::WaylandSlot<Entry> treeDestroy;
    Templates::WaylandSlot<Entry> glassDestroy;
    QRect area;
    QSize clipSize;
    int cornerRadius = -1;
    quint64 fingerprint = 0;
    quint64 revision = 0;
    bool attempted = false;
    QString error;

    explicit Entry(wlr_scene_tree *value) : tree(value) {
      Templates::attachListener(
          &tree->node.events.destroy, treeDestroy, this,
          [](wl_listener *listener, void *) {
            auto *entry = Templates::listenerOwner<Entry>(listener);
            Templates::detachListener(entry->treeDestroy);
            entry->tree = nullptr;
            entry->clearGlass();
          });
    }
    ~Entry() {
      Templates::detachListener(treeDestroy);
      clearGlass();
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
    }
    bool createGlass() {
      if (glass)
        return true;
      glass = wlr_scene_tree_create(tree->node.parent);
      if (!glass)
        return false;
      wlr_scene_node_set_enabled(&glass->node, false);
      Templates::attachListener(
          &glass->node.events.destroy, glassDestroy, this,
          [](wl_listener *listener, void *) {
            auto *entry = Templates::listenerOwner<Entry>(listener);
            Templates::detachListener(entry->glassDestroy);
            entry->glass = nullptr;
            entry->parts.clear();
            entry->clipSize = {};
            entry->cornerRadius = -1;
            if (entry->backing)
              wlr_buffer_unlock(entry->backing);
            entry->backing = nullptr;
            entry->attempted = false;
          });
      return true;
    }

    bool configureClips(QSize size, int radius) {
      radius = std::max(0, std::min({radius, 32, size.width() / 2,
                                     size.height() / 2}));
      if (!parts.empty() && clipSize == size && cornerRadius == radius)
        return true;
      ludash_corner_band bands[LUDASH_CORNER_MAX_BANDS];
      const auto count = ludash_corner_bands(size.width(), size.height(), radius,
                                             bands, LUDASH_CORNER_MAX_BANDS);
      if (!count)
        return false;
      std::vector<wlr_scene_buffer *> next;
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
      clipSize = size;
      cornerRadius = radius;
      attempted = false;
      return true;
    }

    void setBuffer(wlr_buffer *buffer) {
      for (auto *part : parts) {
        const wlr_fbox crop{
            double(part->node.x) * buffer->width / clipSize.width(),
            double(part->node.y) * buffer->height / clipSize.height(),
            double(part->dst_width) * buffer->width / clipSize.width(),
            double(part->dst_height) * buffer->height / clipSize.height()};
        wlr_scene_buffer_set_source_box(part, &crop);
        wlr_scene_buffer_set_buffer(part, buffer);
      }
    }
  };

  wlr_renderer *renderer;
  wlr_allocator *allocator;
  wlr_scene_node *root;
  Templates::WaylandSlot<Impl> rootDestroy;
  std::unordered_map<wlr_scene_tree *, std::unique_ptr<Entry>> entries;
  std::unordered_map<wlr_surface *, std::unique_ptr<SurfaceWatch>> watches;
  bool enabled = true;
  int radius = 18;
  float opacity = 0.96f;
  bool ready = false;
  bool failed = false;
  quint64 frames = 0;
  QString error;

  Impl(wlr_renderer *rendererValue, wlr_allocator *allocatorValue,
        wlr_scene_node *rootValue)
      : renderer(rendererValue), allocator(allocatorValue), root(rootValue) {
    if (root)
      Templates::attachListener(
          &root->events.destroy, rootDestroy, this,
          [](wl_listener *listener, void *) {
            auto *self = Templates::listenerOwner<Impl>(listener);
            Templates::detachListener(self->rootDestroy);
            self->entries.clear();
            self->watches.clear();
            self->root = nullptr;
          });
  }
  ~Impl() {
    Templates::detachListener(rootDestroy);
    entries.clear();
    watches.clear();
  }

  quint64 surfaceRevision(wlr_surface *surface) {
    auto &watch = watches[surface];
    if (!watch || watch->surface != surface)
      watch = std::make_unique<SurfaceWatch>(surface);
    return watch->revision;
  }

  // Return true at the target window: only earlier painter-order nodes belong
  // in its backdrop. Our own sibling underlay is excluded from the cache key.
  bool fingerprint(wlr_scene_node *node, const Entry &entry, Fingerprint &hash,
                    const QRect &sample, int x = 0, int y = 0, int depth = 0) {
    if (node == &entry.tree->node)
      return true;
    if (!node || !node->enabled || node == &entry.glass->node || depth > 128)
      return false;
    x += node->x;
    y += node->y;
    if (node->type == WLR_SCENE_NODE_TREE) {
      auto *tree = wlr_scene_tree_from_node(node);
      wlr_scene_node *child = nullptr;
      wl_list_for_each(child, &tree->children, link)
        if (fingerprint(child, entry, hash, sample, x, y, depth + 1))
          return true;
    } else if (node->type == WLR_SCENE_NODE_RECT) {
      const auto *rect = wlr_scene_rect_from_node(node);
      if (!sample.intersects(QRect(x, y, rect->width, rect->height)))
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
          !sample.intersects(QRect(x, y, buffer->dst_width, buffer->dst_height)))
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
        hash.add(surfaceRevision(surface->surface));
      } else {
        // Keep lower glass stable after scene texture upload releases its
        // buffer pointer. A successful lower capture increments its revision.
        bool managed = false;
        for (const auto &[tree, candidate] : entries) {
          Q_UNUSED(tree);
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
    if (!node || !node->enabled || node->type != WLR_SCENE_NODE_TREE || depth > 128)
      return;
    auto *tree = wlr_scene_tree_from_node(node);
    const auto found = entries.find(tree);
    if (found != entries.end() && found->second->tree)
      ordered.push_back(found->second.get());
    wlr_scene_node *child = nullptr;
    wl_list_for_each(child, &tree->children, link)
      collect(child, ordered, depth + 1);
  }

  void capture(Entry &entry) {
    Fingerprint hash;
    hash.add(entry.area.x());
    hash.add(entry.area.y());
    hash.add(entry.area.width());
    hash.add(entry.area.height());
    hash.add(radius);
    hash.add(entry.cornerRadius);
    const wlr_box area{entry.area.x(), entry.area.y(), entry.area.width(),
                       entry.area.height()};
    wlr_box sample{};
    ludash_scene_backdrop_sample_box(&area, radius, &sample);
    const bool found = fingerprint(root, entry, hash,
                                  QRect(sample.x, sample.y, sample.width, sample.height));
    if (!found)
      return;
    if (!entry.attempted || entry.fingerprint != hash.value) {
      entry.attempted = true;
      entry.fingerprint = hash.value;
      auto *buffer = ludash_scene_backdrop_render(
          renderer, allocator, root, &entry.tree->node, &entry.glass->node,
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
      } else {
        entry.error = "The renderer could not create a window backdrop.";
      }
    }
    const bool available = entry.backing && entry.error.isEmpty();
    if (entry.glass->node.enabled != available)
      wlr_scene_node_set_enabled(&entry.glass->node, available);
    ready = ready || available;
    if (!entry.error.isEmpty()) {
      failed = true;
      error = entry.error;
    }
  }
};

WindowGlass::WindowGlass(wlr_renderer *renderer, wlr_allocator *allocator,
                          wlr_scene_node *root)
    : d(std::make_unique<Impl>(renderer, allocator, root)) {}
WindowGlass::~WindowGlass() = default;

bool WindowGlass::configure(bool blurEnabled, int radius, float opacity) {
  radius = std::clamp(radius, 0, 32);
  opacity = std::clamp(opacity, 0.0f, 1.0f);
  const bool changed = d->enabled != blurEnabled || d->radius != radius ||
                       d->opacity != opacity;
  d->enabled = blurEnabled;
  d->radius = radius;
  d->opacity = opacity;
  return changed;
}

void WindowGlass::update(const QList<Surface> &surfaces, bool animationsActive) {
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
    // Animation owns its temporary alpha and records this steady-state alpha
    // as its baseline. Never replace an in-progress fade with a preference.
    if (!animationsActive) {
      float opacity = surface.enabled ? d->opacity : 1.0f;
      wlr_scene_node_for_each_buffer(&tree->node, setWindowOpacity, &opacity);
    }
    int x = 0, y = 0;
    if (!d->enabled || d->radius == 0 || !surface.enabled ||
        !surface.size.isValid() || !tree->node.parent ||
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
    if (entry->glass->node.parent != tree->node.parent)
      wlr_scene_node_reparent(&entry->glass->node, tree->node.parent);
    if (tree->node.link.prev != &entry->glass->node.link)
      wlr_scene_node_place_below(&entry->glass->node, &tree->node);
    if (entry->glass->node.x != tree->node.x ||
        entry->glass->node.y != tree->node.y)
      wlr_scene_node_set_position(&entry->glass->node, tree->node.x, tree->node.y);
    entry->area = QRect(QPoint(x, y), surface.size);
  }
  std::erase_if(d->entries, [&active](const auto &item) {
    return !active.contains(item.first) || !item.second->tree;
  });
  std::erase_if(d->watches, [](const auto &item) {
    return !item.second->surface;
  });
  if (d->entries.empty()) {
    d->watches.clear();
    return;
  }
  std::vector<Impl::Entry *> ordered;
  d->collect(d->root, ordered);
  for (auto *entry : ordered)
    if (entry->glass)
      d->capture(*entry);
}

bool WindowGlass::ready() const { return d->ready; }
bool WindowGlass::failed() const { return d->failed; }
quint64 WindowGlass::frames() const { return d->frames; }
QString WindowGlass::error() const { return d->error; }

} // namespace LunaDash
