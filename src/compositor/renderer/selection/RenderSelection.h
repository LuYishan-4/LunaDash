#pragma once

#include <stdbool.h>

struct wlr_backend;
struct wlr_renderer;

#ifdef __cplusplus
namespace LunaDash {
extern "C" {
#endif

// Selection and capability reporting are shared by all wlroots renderers.
bool ludash_renderer_preference_valid(const char *preference);
bool ludash_renderer_vulkan_available(void);
struct wlr_renderer *ludash_renderer_create(struct wlr_backend *backend,
                                          const char *preference);
const char *ludash_renderer_name(struct wlr_renderer *renderer);

#ifdef __cplusplus
}
} // namespace LunaDash
#endif
