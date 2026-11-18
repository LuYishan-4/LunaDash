#pragma once

struct wlr_renderer;
struct wlr_allocator;
struct wlr_scene_node;
struct wlr_buffer;
struct wlr_box;
struct wl_event_loop;

#ifdef __cplusplus
namespace LunaDash {
extern "C" {
#endif

/* Render only the enabled scene content below stop, excluding skip (the old
 * backdrop). Blur that content, never the application's text. The returned
 * producer-owned buffer covers area at a bounded, reduced resolution; callers
 * set its scene destination size to area and eventually wlr_buffer_drop it.
 * Invalid geometry, missing stop, unsupported imports and renderer failures
 * return NULL. Explicitly synchronized sources are read after their acquire
 * point and stay locked until the GPU is done with them (a timeline waiter on
 * `loop`), so their release waits for this read; without timeline support
 * they make the call return NULL. No scene state or client buffer is
 * modified. */
struct wlr_buffer *ludash_scene_backdrop_render(
    struct wlr_renderer *renderer, struct wlr_allocator *allocator,
    struct wl_event_loop *loop, struct wlr_scene_node *root,
    struct wlr_scene_node *stop, struct wlr_scene_node *skip,
    const struct wlr_box *area, int radius);

/* Drops the scratch buffers kept between captures (before the allocator goes). */
void ludash_scene_backdrop_finish(void);

/* Conservative scene-space bounds sampled by the reduced-resolution blur,
 * including its filter padding. Used for cache invalidation. */
void ludash_scene_backdrop_sample_box(const struct wlr_box *area, int radius,
                                     struct wlr_box *sample);

#ifdef __cplusplus
}
}
#endif
