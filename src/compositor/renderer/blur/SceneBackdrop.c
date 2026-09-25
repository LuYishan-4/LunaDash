#include "compositor/renderer/blur/SceneBackdrop.h"
#include <drm_fourcc.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#if __has_include(<wlr/util/transform.h>)
#include <wlr/util/transform.h>
#else
#include <wlr/types/wlr_output.h>
#endif
#include <wlr/version.h>
#if WLR_VERSION_MINOR >= 19
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#endif

enum { LUDASH_BACKDROP_LIMIT = 8192, LUDASH_BACKDROP_SIZE = 1024 };

struct ludash_backdrop_capture {
  struct wlr_renderer *renderer;
  struct wlr_render_pass *pass;
  struct wlr_scene_node *stop;
  struct wlr_scene_node *skip;
  struct wlr_texture **imports;
  size_t import_count;
  size_t visited;
  double x, y, scale_x, scale_y;
  int width, height;
  bool found, failed;
};

static struct wlr_buffer *ludash_backdrop_buffer(
    struct wlr_renderer *renderer, struct wlr_allocator *allocator,
    int width, int height) {
  const struct wlr_drm_format_set *formats = NULL;
#if WLR_VERSION_MINOR >= 20
  formats = wlr_renderer_get_texture_formats(renderer, allocator->buffer_caps);
#else
  (void)renderer;
#endif
  const struct wlr_drm_format *format = formats
      ? wlr_drm_format_set_get(formats, DRM_FORMAT_ARGB8888) : NULL;
  struct wlr_drm_format_set fallback = {0};
  // Older renderer APIs and data-pointer allocators use the same portable
  // ARGB target as the existing thumbnail renderer. Allocation failure is
  // reported instead of guessing a driver-specific modifier.
  if (!format) {
    wlr_drm_format_set_add(&fallback, DRM_FORMAT_ARGB8888,
                           DRM_FORMAT_MOD_LINEAR);
    wlr_drm_format_set_add(&fallback, DRM_FORMAT_ARGB8888,
                           DRM_FORMAT_MOD_INVALID);
    format = wlr_drm_format_set_get(&fallback, DRM_FORMAT_ARGB8888);
  }
  struct wlr_buffer *buffer = format
      ? wlr_allocator_create_buffer(allocator, width, height, format) : NULL;
  wlr_drm_format_set_finish(&fallback);
  return buffer;
}

static struct wlr_box ludash_backdrop_box(
    const struct ludash_backdrop_capture *capture,
    double x, double y, double width, double height) {
  const int left = (int)floor((x - capture->x) * capture->scale_x);
  const int top = (int)floor((y - capture->y) * capture->scale_y);
  const int right = (int)ceil((x + width - capture->x) * capture->scale_x);
  const int bottom = (int)ceil((y + height - capture->y) * capture->scale_y);
  return (struct wlr_box){left, top, right - left, bottom - top};
}

static bool ludash_backdrop_intersects(
    const struct ludash_backdrop_capture *capture, struct wlr_box box) {
  return box.width > 0 && box.height > 0 && box.x < capture->width &&
         box.y < capture->height && box.x + box.width > 0 &&
         box.y + box.height > 0;
}

static void ludash_backdrop_visit(struct ludash_backdrop_capture *capture,
                                  struct wlr_scene_node *node,
                                  double x, double y, unsigned depth) {
  if (capture->found || capture->failed)
    return;
  if (node == capture->stop) {
    capture->found = true;
    return;
  }
  if (node == capture->skip || !node->enabled)
    return;
  if (depth > 128 || ++capture->visited > LUDASH_BACKDROP_LIMIT) {
    capture->failed = true;
    return;
  }
  x += node->x;
  y += node->y;
  if (node->type == WLR_SCENE_NODE_TREE) {
    struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
    struct wlr_scene_node *child;
    wl_list_for_each(child, &tree->children, link)
      ludash_backdrop_visit(capture, child, x, y, depth + 1);
  } else if (node->type == WLR_SCENE_NODE_RECT) {
    struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
    const struct wlr_box box = ludash_backdrop_box(
        capture, x, y, rect->width, rect->height);
    if (!ludash_backdrop_intersects(capture, box))
      return;
    const struct wlr_render_rect_options options = {
        .box = box,
        .color = {rect->color[0], rect->color[1], rect->color[2], rect->color[3]}};
    wlr_render_pass_add_rect(capture->pass, &options);
  } else if (node->type == WLR_SCENE_NODE_BUFFER) {
    struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
    if (buffer->opacity <= 0)
      return;
    // An off-area source with known destination geometry cannot contribute
    // pixels, and an unsupported import must not fail an unrelated window.
    if (buffer->dst_width > 0 && buffer->dst_height > 0) {
      const struct wlr_box bounds = ludash_backdrop_box(
          capture, x, y, buffer->dst_width, buffer->dst_height);
      if (!ludash_backdrop_intersects(capture, bounds))
        return;
    }
    struct wlr_scene_surface *surface =
        wlr_scene_surface_try_from_buffer(buffer);
    struct wlr_texture *texture = surface
        ? wlr_surface_get_texture(surface->surface) : NULL;
    bool imported = false;
    if (!texture && buffer->buffer) {
      texture = wlr_texture_from_buffer(capture->renderer, buffer->buffer);
      imported = texture != NULL;
    }
    if (!texture) {
      // An empty placeholder buffer does not have pixels to render.
      if (buffer->buffer || surface)
        capture->failed = true;
      return;
    }
    if (imported)
      capture->imports[capture->import_count++] = texture;
    int width = buffer->dst_width, height = buffer->dst_height;
    if (width <= 0 || height <= 0) {
      width = texture->width;
      height = texture->height;
      if (buffer->transform & 1) {
        const int swapped = width;
        width = height;
        height = swapped;
      }
    }
    const struct wlr_box box = ludash_backdrop_box(capture, x, y, width, height);
    if (!ludash_backdrop_intersects(capture, box))
      return;
    const struct wlr_render_texture_options options = {
        .texture = texture,
        .src_box = buffer->src_box,
        .dst_box = box,
        .alpha = &buffer->opacity,
        .transform = wlr_output_transform_invert(buffer->transform),
        .filter_mode = WLR_SCALE_FILTER_BILINEAR};
#if WLR_VERSION_MINOR >= 19
    if (surface) {
      struct wlr_linux_drm_syncobj_surface_v1_state *sync =
          wlr_linux_drm_syncobj_v1_get_surface_state(surface->surface);
      if (sync && sync->acquire_timeline) {
        // Capturing a source adds a new use beyond the scene output pass.
        // An acquire wait does not keep the client's release point pending.
        // This synchronous helper cannot retain a failed GPU submission and
        // its release merger, so refuse the optional effect before queuing
        // an explicit-sync read. The target itself is excluded above and may
        // still use a backdrop made entirely from implicit-sync sources.
        capture->failed = true;
        return;
      }
    }
#endif
    wlr_render_pass_add_texture(capture->pass, &options);
  }
}

static bool ludash_backdrop_blur(struct wlr_renderer *renderer,
                                 struct wlr_buffer *source,
                                 struct wlr_buffer *destination,
                                 double radius, int padding, bool horizontal) {
  struct wlr_texture *texture = wlr_texture_from_buffer(renderer, source);
  if (!texture)
    return false;
  struct wlr_render_pass *pass =
      wlr_renderer_begin_buffer_pass(renderer, destination, NULL);
  if (!pass) {
    wlr_texture_destroy(texture);
    return false;
  }
  static const float weights[9] = {
      0.0162162162f, 0.0540540541f, 0.1216216216f, 0.1945945946f,
      0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f,
      0.0162162162f};
  float total = 0;
  for (int tap = 0; tap < 9; ++tap) {
    total += weights[tap];
    // Running weighted average of an opaque capture. This uses the renderer's
    // existing premultiplied blend operation, with no GL context substitution.
    const float alpha = weights[tap] / total;
    const double offset = padding + (tap - 4) * radius / 4.0;
    const struct wlr_render_texture_options options = {
        .texture = texture,
        .src_box = {horizontal ? offset : 0, horizontal ? 0 : offset,
                    destination->width, destination->height},
        .dst_box = {0, 0, destination->width, destination->height},
        .alpha = &alpha,
        .filter_mode = WLR_SCALE_FILTER_BILINEAR,
        .blend_mode = tap == 0 ? WLR_RENDER_BLEND_MODE_NONE
                              : WLR_RENDER_BLEND_MODE_PREMULTIPLIED};
    wlr_render_pass_add_texture(pass, &options);
  }
  const bool ok = wlr_render_pass_submit(pass);
  wlr_texture_destroy(texture);
  return ok;
}

struct wlr_buffer *ludash_scene_backdrop_render(
    struct wlr_renderer *renderer, struct wlr_allocator *allocator,
    struct wlr_scene_node *root, struct wlr_scene_node *stop,
    struct wlr_scene_node *skip, const struct wlr_box *area, int radius) {
  if (!renderer || !allocator || !root || !stop || !area ||
      radius < 1 || radius > 32 || area->width < 1 || area->height < 1 ||
      area->width > 16384 || area->height > 16384 ||
      area->x < -65536 || area->x > 65536 ||
      area->y < -65536 || area->y > 65536 || root == stop || skip == stop)
    return NULL;
  const double scale = fmax(4.0, fmax((double)area->width / (LUDASH_BACKDROP_SIZE - 20),
                                    (double)area->height / (LUDASH_BACKDROP_SIZE - 20)));
  const int width = (int)ceil(area->width / scale);
  const int height = (int)ceil(area->height / scale);
  const double scale_x = (double)width / area->width;
  const double scale_y = (double)height / area->height;
  const int padding_x = (int)ceil(radius * scale_x) + 2;
  const int padding_y = (int)ceil(radius * scale_y) + 2;
  struct wlr_buffer *capture_buffer = ludash_backdrop_buffer(
      renderer, allocator, width + 2 * padding_x, height + 2 * padding_y);
  struct wlr_buffer *horizontal = ludash_backdrop_buffer(
      renderer, allocator, width, height + 2 * padding_y);
  struct wlr_buffer *result = ludash_backdrop_buffer(renderer, allocator, width, height);
  struct wlr_texture **imports = calloc(LUDASH_BACKDROP_LIMIT, sizeof(*imports));
  bool ok = false;
  if (!capture_buffer || !horizontal || !result || !imports)
    goto cleanup;
  struct wlr_render_pass *pass =
      wlr_renderer_begin_buffer_pass(renderer, capture_buffer, NULL);
  if (!pass)
    goto cleanup;
  const struct wlr_render_rect_options clear = {
      .box = {0, 0, capture_buffer->width, capture_buffer->height},
      .color = {0.043f, 0.067f, 0.078f, 1.0f},
      .blend_mode = WLR_RENDER_BLEND_MODE_NONE};
  wlr_render_pass_add_rect(pass, &clear);
  struct ludash_backdrop_capture capture = {
      .renderer = renderer, .pass = pass, .stop = stop, .skip = skip,
      .imports = imports, .x = area->x - padding_x / scale_x,
      .y = area->y - padding_y / scale_y, .scale_x = scale_x, .scale_y = scale_y,
      .width = capture_buffer->width, .height = capture_buffer->height};
  ludash_backdrop_visit(&capture, root, 0, 0, 0);
  const bool submitted = wlr_render_pass_submit(pass);
  for (size_t index = 0; index < capture.import_count; ++index)
    wlr_texture_destroy(imports[index]);
  if (!submitted || !capture.found || capture.failed)
    goto cleanup;
  ok = ludash_backdrop_blur(renderer, capture_buffer, horizontal,
                            radius * scale_x, padding_x, true) &&
       ludash_backdrop_blur(renderer, horizontal, result,
                            radius * scale_y, padding_y, false);
cleanup:
  free(imports);
  if (capture_buffer)
    wlr_buffer_drop(capture_buffer);
  if (horizontal)
    wlr_buffer_drop(horizontal);
  if (!ok && result) {
    wlr_buffer_drop(result);
    result = NULL;
  }
  return result;
}
