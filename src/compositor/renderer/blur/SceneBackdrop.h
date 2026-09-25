#pragma once

struct wlr_renderer;
struct wlr_allocator;
struct wlr_scene_node;
struct wlr_buffer;
struct wlr_box;

#ifdef __cplusplus
namespace LunaDash {
extern "C" {
#endif

/* Render only the enabled scene content below stop, excluding skip (the old
 * backdrop). Blur that content, never the application's text. The returned
 * producer-owned buffer covers area at a bounded, reduced resolution; callers
 * set its scene destination size to area and eventually wlr_buffer_drop it.
 * Invalid geometry, missing stop, unsupported imports and renderer failures
 * return NULL. On wlroots >= 0.19 an intersecting lower source using explicit
 * sync also returns NULL: this helper does not own asynchronous commit-release
 * completion. An explicit-sync target may still blur implicit-sync content
 * below it because the target is never sampled. No scene state or client
 * buffer is modified. */
struct wlr_buffer *ludash_scene_backdrop_render(
    struct wlr_renderer *renderer, struct wlr_allocator *allocator,
    struct wlr_scene_node *root, struct wlr_scene_node *stop,
    struct wlr_scene_node *skip, const struct wlr_box *area, int radius);

#ifdef __cplusplus
}
}
#endif
