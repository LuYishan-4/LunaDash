#include "compositor/wayland/SessionLock.hpp"
#include "compositor/wayland/WaylandHook.hpp"

// C++ library headers must be parsed before the wlroots compatibility macros.
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <list>
#include <utility>

#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

namespace LunaDash {
namespace {

// Opaque black: what a locked output shows where no lock surface is.
constexpr float kBlank[4] = {0.0f, 0.0f, 0.0f, 1.0f};

} // namespace

struct SessionLock::State {
  struct Surface {
    wlr_session_lock_surface_v1 *lock = nullptr;
    wlr_scene_tree *tree = nullptr;
    WaylandHook destroy;
    WaylandHook map;
  };

  SessionLock::Callbacks callbacks;
  wlr_session_lock_manager_v1 *manager = nullptr;
  wlr_scene_tree *layer = nullptr;
  wlr_session_lock_v1 *lock = nullptr;
  std::vector<SessionLock::Output> outputs;
  std::vector<wlr_scene_rect *> blanks;
  std::list<Surface> surfaces;
  bool *locked = nullptr;
  bool focused = false;
  WaylandHook newLock, managerDestroy, newSurface, unlock, lockDestroy;

  const SessionLock::Output *outputFor(wlr_output *output) const {
    for (const auto &entry : outputs)
      if (entry.output == output)
        return &entry;
    return nullptr;
  }

  void layoutBlanks() {
    for (auto *blank : blanks)
      wlr_scene_node_destroy(&blank->node);
    blanks.clear();
    if (!layer)
      return;
    for (const auto &output : outputs) {
      auto *blank = wlr_scene_rect_create(layer, std::max(1, output.width),
                                          std::max(1, output.height), kBlank);
      if (!blank)
        continue;
      wlr_scene_node_set_position(&blank->node, output.x, output.y);
      // Behind the lock surfaces.
      wlr_scene_node_lower_to_bottom(&blank->node);
      blanks.push_back(blank);
    }
  }

  void placeSurface(Surface &surface) {
    const auto *output = outputFor(surface.lock->output);
    if (!output || !surface.tree)
      return;
    wlr_scene_node_set_position(&surface.tree->node, output->x, output->y);
    wlr_session_lock_surface_v1_configure(
        surface.lock, static_cast<std::uint32_t>(std::max(1, output->width)),
        static_cast<std::uint32_t>(std::max(1, output->height)));
  }

  // The first mapped lock surface gets the keyboard.
  void focusFirst() {
    for (auto &surface : surfaces)
      if (surface.lock->surface && surface.lock->surface->mapped) {
        callbacks.focus(surface.lock->surface);
        focused = true;
        return;
      }
    callbacks.focus(nullptr);
    focused = false;
  }

  void addSurface(wlr_session_lock_surface_v1 *lock) {
    auto &surface = surfaces.emplace_back();
    surface.lock = lock;
    surface.tree = wlr_scene_subsurface_tree_create(layer, lock->surface);
    placeSurface(surface);
    auto *entry = &surface;
    surface.map.attach(&lock->surface->events.map, [this](void *) {
      if (!focused)
        focusFirst();
    });
    surface.destroy.attach(&lock->events.destroy, [this, entry](void *) {
      // The scene tree goes with the wlr_surface; forget it.
      const bool hadFocus = focused;
      surfaces.remove_if([entry](const Surface &s) { return &s == entry; });
      if (hadFocus)
        focusFirst();
    });
  }

  void dropLock() {
    newSurface.detach();
    unlock.detach();
    lockDestroy.detach();
    lock = nullptr;
    surfaces.clear();
    focused = false;
  }

  void onNewLock(wlr_session_lock_v1 *candidate) {
    // One locker at a time; a dead locker's session may be taken over.
    if (lock) {
      wlr_session_lock_v1_destroy(candidate);
      return;
    }
    const bool wasLocked = *locked;
    lock = candidate;
    *locked = true;
    wlr_scene_node_raise_to_top(&layer->node);
    wlr_scene_node_set_enabled(&layer->node, true);
    layoutBlanks();
    callbacks.focus(nullptr);
    newSurface.attach(&lock->events.new_surface, [this](void *data) {
      addSurface(static_cast<wlr_session_lock_surface_v1 *>(data));
    });
    unlock.attach(&lock->events.unlock, [this](void *) {
      dropLock();
      *locked = false;
      wlr_scene_node_set_enabled(&layer->node, false);
      callbacks.changed(false);
    });
    lockDestroy.attach(&lock->events.destroy, [this](void *) {
      // Died without unlocking: stay locked and blank.
      dropLock();
    });
    // Every output is covered from the next frame on.
    wlr_session_lock_v1_send_locked(lock);
    if (!wasLocked)
      callbacks.changed(true);
  }
};

SessionLock::SessionLock(wl_display *display, wlr_scene_tree *root,
                         Callbacks callbacks)
    : d(std::make_unique<State>()) {
  d->callbacks = std::move(callbacks);
  d->locked = &locked_;
  d->manager = wlr_session_lock_manager_v1_create(display);
  if (!d->manager || !root)
    return;
  d->layer = wlr_scene_tree_create(root);
  if (d->layer)
    wlr_scene_node_set_enabled(&d->layer->node, false);
  d->newLock.attach(&d->manager->events.new_lock, [this](void *data) {
    d->onNewLock(static_cast<wlr_session_lock_v1 *>(data));
  });
  d->managerDestroy.attach(&d->manager->events.destroy, [this](void *) {
    d->newLock.detach();
    d->managerDestroy.detach();
    d->manager = nullptr;
  });
}

SessionLock::~SessionLock() = default;

bool SessionLock::valid() const { return d->manager && d->layer; }

bool SessionLock::isLockSurface(wlr_surface *surface) const {
  if (!surface)
    return false;
  auto *root = wlr_surface_get_root_surface(surface);
  for (const auto &entry : d->surfaces)
    if (entry.lock->surface == root)
      return true;
  return false;
}

wlr_surface *SessionLock::surfaceAt(double lx, double ly, double *sx,
                                    double *sy) const {
  if (!d->layer || !locked_)
    return nullptr;
  auto *node = wlr_scene_node_at(&d->layer->node, lx, ly, sx, sy);
  if (!node || node->type != WLR_SCENE_NODE_BUFFER)
    return nullptr;
  auto *scene = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
  return scene && isLockSurface(scene->surface) ? scene->surface : nullptr;
}

void SessionLock::setOutputs(const std::vector<Output> &outputs) {
  d->outputs = outputs;
  if (!locked_)
    return;
  d->layoutBlanks();
  for (auto &surface : d->surfaces)
    d->placeSurface(surface);
}

} // namespace LunaDash
