#include "compositor/renderer/capture/ThumbnailReadback.h"
#include <drm_fourcc.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/version.h>

bool ludash_thumbnail_read(struct wlr_renderer *renderer,
                           struct wlr_allocator *allocator,
                           struct wlr_texture *texture,
                           const struct wlr_fbox *source, int transform,
                           int width, int height, uint8_t *pixels) {
  if (!renderer || !allocator || !texture || !pixels || width < 1 ||
      height < 1 || width > 480 || height > 300 || transform < 0 ||
      transform > 7)
    return false;
  struct wlr_drm_format_set formats = {0};
  wlr_drm_format_set_add(&formats, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR);
  wlr_drm_format_set_add(&formats, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_INVALID);
  const struct wlr_drm_format *format =
      wlr_drm_format_set_get(&formats, DRM_FORMAT_ARGB8888);
  struct wlr_buffer *buffer =
      format ? wlr_allocator_create_buffer(allocator, width, height, format)
             : NULL;
  wlr_drm_format_set_finish(&formats);
  if (!buffer)
    return false;
  bool success = false;
  struct wlr_render_pass *pass =
      wlr_renderer_begin_buffer_pass(renderer, buffer, NULL);
  if (!pass)
    goto cleanup;
  struct wlr_render_texture_options options = {
      .texture = texture,
      .dst_box = {0, 0, width, height},
      .transform = (enum wl_output_transform)transform,
      .blend_mode = WLR_RENDER_BLEND_MODE_NONE,
      .filter_mode = WLR_SCALE_FILTER_BILINEAR};
  if (source)
    options.src_box = *source;
  wlr_render_pass_add_texture(pass, &options);
  if (!wlr_render_pass_submit(pass))
    goto cleanup;
#if WLR_VERSION_MINOR >= 18
  struct wlr_texture *result = wlr_texture_from_buffer(renderer, buffer);
  if (result) {
    const struct wlr_texture_read_pixels_options read = {
        .data = pixels,
        .format = DRM_FORMAT_ABGR8888,
        .stride = (uint32_t)width * 4};
    success = wlr_texture_read_pixels(result, &read);
    wlr_texture_destroy(result);
  }
#else
  if (wlr_renderer_begin_with_buffer(renderer, buffer)) {
    success = wlr_renderer_read_pixels(renderer, DRM_FORMAT_ABGR8888,
                                       (uint32_t)width * 4, width, height, 0, 0,
                                       0, 0, pixels);
    wlr_renderer_end(renderer);
  }
#endif
cleanup:
  wlr_buffer_drop(buffer);
  return success;
}
