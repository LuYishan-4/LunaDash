#pragma once
#include <stdbool.h>
#include <stdint.h>
struct wlr_renderer;
struct wlr_allocator;
struct wlr_texture;
struct wlr_fbox;
#ifdef __cplusplus
namespace LunaDash {
extern "C" {
#endif
/* Scale on the renderer before reading a bounded RGBA thumbnail into pixels. */
bool ludash_thumbnail_read(struct wlr_renderer *renderer,
                           struct wlr_allocator *allocator,
                           struct wlr_texture *texture,
                           const struct wlr_fbox *source, int transform,
                           int width, int height, uint8_t *pixels);
#ifdef __cplusplus
}
}
#endif
