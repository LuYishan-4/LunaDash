#include "compositor/wayland/BackgroundEffectManager.hpp"

// C++ library headers must be parsed before the wlroots compatibility macros
// (xcb_ewmh.h, reached through wlr/xwayland.h, includes <stdlib.h>).
#include <algorithm>
#include <cstdlib>
#include <utility>

#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

#include "ext-background-effect-v1-protocol.h"

#if WLR_VERSION_MINOR >= 18

namespace LunaDash {

bool BackgroundEffectManager::supported() { return true; }
namespace {

constexpr int kManagerVersion = 1;

struct BlurState {
  pixman_region32_t region;
};

void initBlurState(void *state) {
  pixman_region32_init(&static_cast<BlurState *>(state)->region);
}

void finishBlurState(void *state) {
  pixman_region32_fini(&static_cast<BlurState *>(state)->region);
}

// The blur region keeps its value until the client sets a new one, so a
// commit copies it instead of resetting the pending state.
void moveBlurState(void *dst, void *src) {
  pixman_region32_copy(&static_cast<BlurState *>(dst)->region,
                       &static_cast<BlurState *>(src)->region);
}

} // namespace

// Lives as long as its wl_surface. The client's effect object may be destroyed
// earlier: removing the effect is double-buffered, so the attachment keeps
// applying commits (with an empty region) until the surface goes away, and a
// later get_background_effect for the same surface reuses it.
struct BackgroundEffectManager::Attachment {
  BackgroundEffectManager *manager = nullptr;
  wlr_surface *surface = nullptr;
  wl_resource *resource = nullptr;
  wlr_surface_synced synced{};
  BlurState pending{};
  BlurState current{};
  bool blurred = false;
  wl_listener surfaceDestroy{};
};

struct BackgroundEffectManager::Callbacks {
  static Attachment *attachmentFrom(wl_resource *resource) {
    return static_cast<Attachment *>(wl_resource_get_user_data(resource));
  }

  static void committed(wlr_surface_synced *synced) {
    Attachment *attachment = wl_container_of(synced, attachment, synced);
    const bool blurred = pixman_region32_not_empty(&attachment->current.region);
    if (std::exchange(attachment->blurred, blurred) != blurred &&
        attachment->manager->changed_)
      attachment->manager->changed_();
  }

  static constexpr wlr_surface_synced_impl syncedImpl = {
      .state_size = sizeof(BlurState),
      .init_state = initBlurState,
      .finish_state = finishBlurState,
      .move_state = moveBlurState,
      .commit = committed,
  };

  static void destroyRequest(wl_client *, wl_resource *resource) {
    wl_resource_destroy(resource);
  }

  static void setBlurRegion(wl_client *, wl_resource *resource,
                            wl_resource *region) {
    auto *attachment = attachmentFrom(resource);
    if (!attachment) {
      wl_resource_post_error(resource,
                             EXT_BACKGROUND_EFFECT_SURFACE_V1_ERROR_SURFACE_DESTROYED,
                             "the wl_surface of this background effect was destroyed");
      return;
    }
    if (region)
      pixman_region32_copy(&attachment->pending.region,
                           wlr_region_from_resource(region));
    else
      pixman_region32_clear(&attachment->pending.region);
  }

  static constexpr struct ext_background_effect_surface_v1_interface surfaceImpl = {
      .destroy = destroyRequest,
      .set_blur_region = setBlurRegion,
  };

  static void effectDestroyed(wl_resource *resource) {
    auto *attachment = attachmentFrom(resource);
    if (!attachment)
      return;
    // Applied with the surface's next commit, as the protocol requires.
    attachment->resource = nullptr;
    pixman_region32_clear(&attachment->pending.region);
  }

  static void surfaceDestroyed(wl_listener *listener, void *) {
    Attachment *attachment = wl_container_of(listener, attachment, surfaceDestroy);
    release(attachment);
  }

  static void release(Attachment *attachment) {
    auto *manager = attachment->manager;
    if (attachment->resource)
      wl_resource_set_user_data(attachment->resource, nullptr);
    wlr_surface_synced_finish(&attachment->synced);
    wl_list_remove(&attachment->surfaceDestroy.link);
    manager->attachments_.erase(attachment->surface);
    const bool blurred = attachment->blurred;
    delete attachment;
    if (blurred && manager->changed_)
      manager->changed_();
  }

  static void getBackgroundEffect(wl_client *client, wl_resource *managerResource,
                                  std::uint32_t id, wl_resource *surfaceResource) {
    auto *manager = static_cast<BackgroundEffectManager *>(
        wl_resource_get_user_data(managerResource));
    auto *surface = wlr_surface_from_resource(surfaceResource);
    auto found = manager->attachments_.find(surface);
    if (found != manager->attachments_.end() && found->second->resource) {
      wl_resource_post_error(
          managerResource, EXT_BACKGROUND_EFFECT_MANAGER_V1_ERROR_BACKGROUND_EFFECT_EXISTS,
          "the wl_surface already has a background effect object");
      return;
    }
    auto *resource = wl_resource_create(client, &ext_background_effect_surface_v1_interface,
                                        wl_resource_get_version(managerResource), id);
    if (!resource) {
      wl_client_post_no_memory(client);
      return;
    }

    Attachment *attachment = nullptr;
    if (found != manager->attachments_.end()) {
      attachment = found->second;
    } else {
      attachment = new Attachment;
      attachment->manager = manager;
      attachment->surface = surface;
      if (!wlr_surface_synced_init(&attachment->synced, surface, &syncedImpl,
                                   &attachment->pending, &attachment->current)) {
        delete attachment;
        wl_resource_destroy(resource);
        wl_client_post_no_memory(client);
        return;
      }
      attachment->surfaceDestroy.notify = surfaceDestroyed;
      wl_signal_add(&surface->events.destroy, &attachment->surfaceDestroy);
      manager->attachments_.emplace(surface, attachment);
    }
    attachment->resource = resource;
    wl_resource_set_implementation(resource, &surfaceImpl, attachment, effectDestroyed);
  }

  static constexpr struct ext_background_effect_manager_v1_interface managerImpl = {
      .destroy = destroyRequest,
      .get_background_effect = getBackgroundEffect,
  };

  static void managerDestroyed(wl_resource *resource) {
    auto *manager = static_cast<BackgroundEffectManager *>(wl_resource_get_user_data(resource));
    std::erase(manager->managers_, resource);
  }

  static void bind(wl_client *client, void *data, std::uint32_t version, std::uint32_t id) {
    auto *manager = static_cast<BackgroundEffectManager *>(data);
    auto *resource = wl_resource_create(client, &ext_background_effect_manager_v1_interface,
                                        static_cast<int>(version), id);
    if (!resource) {
      wl_client_post_no_memory(client);
      return;
    }
    wl_resource_set_implementation(resource, &managerImpl, manager, managerDestroyed);
    manager->managers_.push_back(resource);
    ext_background_effect_manager_v1_send_capabilities(
        resource, manager->blurAvailable_ ? EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR : 0);
  }
};

BackgroundEffectManager::BackgroundEffectManager(wl_display *display,
                                                 std::function<void()> changed)
    : changed_(std::move(changed)) {
  global_ = wl_global_create(display, &ext_background_effect_manager_v1_interface,
                             kManagerVersion, this, Callbacks::bind);
}

BackgroundEffectManager::~BackgroundEffectManager() {
  changed_ = nullptr;
  // Clients are normally destroyed first; release whatever is left so no
  // listener or synced state outlives the manager.
  while (!attachments_.empty())
    Callbacks::release(attachments_.begin()->second);
  for (auto *resource : managers_)
    wl_resource_set_destructor(resource, nullptr);
  if (global_)
    wl_global_destroy(global_);
}

bool BackgroundEffectManager::wantsBlur(wlr_surface *surface) const {
  if (!blurAvailable_ || !surface)
    return false;
  const auto found = attachments_.find(surface);
  return found != attachments_.end() && found->second->blurred;
}

void BackgroundEffectManager::setBlurAvailable(bool available) {
  if (blurAvailable_ == available)
    return;
  blurAvailable_ = available;
  for (auto *resource : managers_)
    ext_background_effect_manager_v1_send_capabilities(
        resource, available ? EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR : 0);
}

} // namespace LunaDash

#else

// wlroots before 0.18 has no wlr_surface_synced to keep the double-buffered
// blur region in step with surface commits: the protocol is not offered.
namespace LunaDash {

bool BackgroundEffectManager::supported() { return false; }

BackgroundEffectManager::BackgroundEffectManager(wl_display *,
                                                 std::function<void()> changed)
    : changed_(std::move(changed)) {}

BackgroundEffectManager::~BackgroundEffectManager() = default;

bool BackgroundEffectManager::wantsBlur(wlr_surface *) const { return false; }

void BackgroundEffectManager::setBlurAvailable(bool available) {
  blurAvailable_ = available;
}

} // namespace LunaDash

#endif
