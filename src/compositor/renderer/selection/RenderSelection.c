#define _POSIX_C_SOURCE 200809L
#include "compositor/renderer/selection/RenderSelection.h"

#include <stdlib.h>
#include <string.h>
#include <wlr/config.h>
#include <wlr/render/pixman.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>

#if WLR_HAS_GLES2_RENDERER
#include <wlr/render/gles2.h>
#endif
#if LUDASH_HAS_VULKAN_RENDERER
#include <wlr/render/vulkan.h>
#endif

bool ludash_renderer_preference_valid(const char *preference) {
  return preference && (!strcmp(preference, "auto") ||
                        !strcmp(preference, "opengl") ||
                        !strcmp(preference, "gles") ||
                        !strcmp(preference, "vulkan"));
}

bool ludash_renderer_vulkan_available(void) {
  return LUDASH_HAS_VULKAN_RENDERER != 0;
}

const char *ludash_renderer_name(struct wlr_renderer *renderer) {
  if (!renderer)
    return "unavailable";
#if LUDASH_HAS_VULKAN_RENDERER
  if (wlr_renderer_is_vk(renderer))
    return "vulkan";
#endif
#if WLR_HAS_GLES2_RENDERER
  if (wlr_renderer_is_gles2(renderer))
    return "gles2";
#endif
  return wlr_renderer_is_pixman(renderer) ? "pixman" : "unknown";
}

struct wlr_renderer *ludash_renderer_create(struct wlr_backend *backend,
                                          const char *preference) {
  if (!ludash_renderer_preference_valid(preference))
    return NULL;
  const char *environment = getenv("WLR_RENDERER");
  const bool explicit_environment = environment && *environment;
  const char *requested = explicit_environment ? environment : preference;
  if (!strcmp(requested, "vulkan") && !ludash_renderer_vulkan_available()) {
    wlr_log(WLR_ERROR, "Vulkan renderer is unavailable in this build of wlroots/LunaDash.");
    return NULL;
  }
  if (!explicit_environment && strcmp(preference, "auto")) {
    const char *name = !strcmp(preference, "vulkan") ? "vulkan" : "gles2";
    if (setenv("WLR_RENDERER", name, 1) < 0)
      return NULL;
  }
  struct wlr_renderer *renderer = wlr_renderer_autocreate(backend);
  // An explicit Vulkan request must not silently turn into a pixman session.
  if (!strcmp(requested, "vulkan") &&
      strcmp(ludash_renderer_name(renderer), "vulkan")) {
    if (renderer)
      wlr_renderer_destroy(renderer);
    wlr_log(WLR_ERROR, "Vulkan initialization failed; check the DRM render node, "
                       "Vulkan ICD and external-memory support, or select --graphics auto.");
    return NULL;
  }
  if (renderer)
    wlr_log(WLR_INFO, "LunaDash renderer: %s", ludash_renderer_name(renderer));
  return renderer;
}
