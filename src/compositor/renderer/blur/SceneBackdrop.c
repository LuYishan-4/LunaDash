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
#include <wlr/render/drm_syncobj.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#define LUDASH_BACKDROP_TIMELINES 1
#else
#define LUDASH_BACKDROP_TIMELINES 0
#endif

enum { LUDASH_BACKDROP_LIMIT = 8192, LUDASH_BACKDROP_SIZE = 768 };

struct ludash_backdrop_hold {
#if LUDASH_BACKDROP_TIMELINES
  struct wlr_drm_syncobj_timeline_waiter waiter; // first: the callback casts
  struct wlr_drm_syncobj_timeline *timeline;
#endif
  struct wlr_buffer **buffers;
  size_t count;
};

struct ludash_backdrop_capture {
  struct wlr_renderer *renderer;
  struct wlr_render_pass *pass;
  struct wlr_scene_node *stop;
  struct wlr_scene_node *skip;
  struct wlr_texture **imports;
  size_t import_count;
  struct ludash_backdrop_hold *hold;
  size_t visited;
  double x, y, scale_x, scale_y;
  int width, height;
  bool found, failed;
};

static void ludash_backdrop_release(struct ludash_backdrop_hold *hold) {
  for (size_t index = 0; index < hold->count; ++index)
    wlr_buffer_unlock(hold->buffers[index]);
#if LUDASH_BACKDROP_TIMELINES
  if (hold->timeline)
    wlr_drm_syncobj_timeline_unref(hold->timeline);
#endif
  free(hold->buffers);
  free(hold);
}

#if LUDASH_BACKDROP_TIMELINES
static void
ludash_backdrop_done(struct wlr_drm_syncobj_timeline_waiter *waiter) {
  struct ludash_backdrop_hold *hold = (struct ludash_backdrop_hold *)waiter;
  wlr_drm_syncobj_timeline_waiter_finish(&hold->waiter);
  ludash_backdrop_release(hold);
}
#endif

static struct wlr_buffer *
ludash_backdrop_buffer(struct wlr_renderer *renderer,
                       struct wlr_allocator *allocator, int width, int height) {
  const struct wlr_drm_format_set *formats = NULL;
#if WLR_VERSION_MINOR >= 20
  formats = wlr_renderer_get_texture_formats(renderer, allocator->buffer_caps);
#else
  (void)renderer;
#endif
  const struct wlr_drm_format *format =
      formats ? wlr_drm_format_set_get(formats, DRM_FORMAT_ARGB8888) : NULL;
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
  struct wlr_buffer *buffer =
      format ? wlr_allocator_create_buffer(allocator, width, height, format)
             : NULL;
  wlr_drm_format_set_finish(&fallback);
  return buffer;
}

// Scratch targets (the capture and the two blur buffers) are kept between
// captures: allocating GPU buffers costs far more than the passes themselves.
// The least recently used free one makes room for a new size.
enum { LUDASH_SCRATCH_SLOTS = 9 };
static struct ludash_scratch {
  struct wlr_allocator *allocator;
  struct wlr_buffer *buffer;
  unsigned long used;
  bool busy;
} ludash_scratch[LUDASH_SCRATCH_SLOTS];
static unsigned long ludash_scratch_clock;

static struct wlr_buffer *ludash_scratch_take(struct wlr_renderer *renderer,
                                              struct wlr_allocator *allocator,
                                              int width, int height) {
  struct ludash_scratch *free_slot = NULL;
  for (int i = 0; i < LUDASH_SCRATCH_SLOTS; ++i) {
    struct ludash_scratch *slot = &ludash_scratch[i];
    if (slot->busy)
      continue;
    if (slot->buffer && slot->allocator == allocator &&
        slot->buffer->width == width && slot->buffer->height == height) {
      slot->busy = true;
      slot->used = ++ludash_scratch_clock;
      return slot->buffer;
    }
    if (!free_slot || !slot->buffer ||
        (free_slot->buffer && slot->used < free_slot->used))
      free_slot = slot;
  }
  struct wlr_buffer *buffer =
      ludash_backdrop_buffer(renderer, allocator, width, height);
  if (!buffer || !free_slot)
    return buffer;
  if (free_slot->buffer)
    wlr_buffer_drop(free_slot->buffer);
  *free_slot =
      (struct ludash_scratch){allocator, buffer, ++ludash_scratch_clock, true};
  return buffer;
}

static void ludash_scratch_give(struct wlr_buffer *buffer) {
  if (!buffer)
    return;
  for (int i = 0; i < LUDASH_SCRATCH_SLOTS; ++i)
    if (ludash_scratch[i].buffer == buffer) {
      ludash_scratch[i].busy = false;
      return;
    }
  wlr_buffer_drop(buffer);
}

void ludash_scene_backdrop_finish(void) {
  for (int i = 0; i < LUDASH_SCRATCH_SLOTS; ++i) {
    if (ludash_scratch[i].buffer)
      wlr_buffer_drop(ludash_scratch[i].buffer);
    ludash_scratch[i] = (struct ludash_scratch){0};
  }
}

static struct wlr_box
ludash_backdrop_box(const struct ludash_backdrop_capture *capture, double x,
                    double y, double width, double height) {
  const int left = (int)floor((x - capture->x) * capture->scale_x);
  const int top = (int)floor((y - capture->y) * capture->scale_y);
  const int right = (int)ceil((x + width - capture->x) * capture->scale_x);
  const int bottom = (int)ceil((y + height - capture->y) * capture->scale_y);
  return (struct wlr_box){left, top, right - left, bottom - top};
}

static bool
ludash_backdrop_intersects(const struct ludash_backdrop_capture *capture,
                           struct wlr_box box) {
  return box.width > 0 && box.height > 0 && box.x < capture->width &&
         box.y < capture->height && box.x + box.width > 0 &&
         box.y + box.height > 0;
}

static void ludash_backdrop_visit(struct ludash_backdrop_capture *capture,
                                  struct wlr_scene_node *node, double x,
                                  double y, unsigned depth) {
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
    const struct wlr_box box =
        ludash_backdrop_box(capture, x, y, rect->width, rect->height);
    if (!ludash_backdrop_intersects(capture, box))
      return;
    const struct wlr_render_rect_options options = {
        .box = box,
        .color = {rect->color[0], rect->color[1], rect->color[2],
                  rect->color[3]}};
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
    struct wlr_texture *texture =
        surface ? wlr_surface_get_texture(surface->surface) : NULL;
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
    const struct wlr_box box =
        ludash_backdrop_box(capture, x, y, width, height);
    if (!ludash_backdrop_intersects(capture, box))
      return;
    const struct wlr_render_texture_options options = {
        .texture = texture,
        .src_box = buffer->src_box,
        .dst_box = box,
        .alpha = &buffer->opacity,
        .transform = wlr_output_transform_invert(buffer->transform),
        .filter_mode = WLR_SCALE_FILTER_BILINEAR};
#if LUDASH_BACKDROP_TIMELINES
    if (surface) {
      struct wlr_linux_drm_syncobj_surface_v1_state *sync =
          wlr_linux_drm_syncobj_v1_get_surface_state(surface->surface);
      if (sync && sync->acquire_timeline) {
        // Read only once the client finished drawing (acquire), and keep its
        // buffer locked until our pass completes: wlroots signals the
        // client's release point only when every lock is gone.
        if (!capture->hold || !buffer->buffer ||
            capture->hold->count >= LUDASH_BACKDROP_LIMIT) {
          capture->failed = true;
          return;
        }
        capture->hold->buffers[capture->hold->count++] =
            wlr_buffer_lock(buffer->buffer);
        struct wlr_render_texture_options synced = options;
        synced.wait_timeline = sync->acquire_timeline;
        synced.wait_point = sync->acquire_point;
        wlr_render_pass_add_texture(capture->pass, &synced);
        return;
      }
    }
#endif
    wlr_render_pass_add_texture(capture->pass, &options);
  }
}

// One separable pass of a 9-tap Gaussian whose taps are `spacing` pixels
// apart, from source to destination (same size). The weighted average uses the
// renderer's premultiplied blending, so it works with any wlroots renderer.
static bool ludash_backdrop_blur(struct wlr_renderer *renderer,
                                 struct wlr_buffer *source,
                                 struct wlr_buffer *destination, double spacing,
                                 bool horizontal) {
  struct wlr_texture *texture = wlr_texture_from_buffer(renderer, source);
  if (!texture)
    return false;
  struct wlr_render_pass *pass =
      wlr_renderer_begin_buffer_pass(renderer, destination, NULL);
  if (!pass) {
    wlr_texture_destroy(texture);
    return false;
  }
  static const float weights[9] = {0.0162162162f, 0.0540540541f, 0.1216216216f,
                                   0.1945945946f, 0.2270270270f, 0.1945945946f,
                                   0.1216216216f, 0.0540540541f, 0.0162162162f};
  // The centre tap first: it covers every pixel, replacing what was there.
  static const int order[9] = {4, 3, 5, 2, 6, 1, 7, 0, 8};
  const int size = horizontal ? destination->width : destination->height;
  float total = 0;
  for (int index = 0; index < 9; ++index) {
    const int tap = order[index];
    total += weights[tap];
    const float alpha = weights[tap] / total;
    const double offset = (tap - 4) * spacing;
    // Only the part of the shifted source that lies inside the texture
    // (wlroots requires it): edge pixels miss a few taps, inside the padding.
    const int shift = (int)ceil(fabs(offset));
    if (shift >= size)
      continue;
    const double start = offset > 0 ? offset : shift + offset;
    const int target = offset > 0 ? 0 : shift;
    const int length = size - shift;
    struct wlr_render_texture_options options = {
        .texture = texture,
        .alpha = &alpha,
        .filter_mode = WLR_SCALE_FILTER_BILINEAR,
        .blend_mode = index == 0 ? WLR_RENDER_BLEND_MODE_NONE
                                 : WLR_RENDER_BLEND_MODE_PREMULTIPLIED};
    if (horizontal) {
      options.src_box =
          (struct wlr_fbox){start, 0, length, destination->height};
      options.dst_box =
          (struct wlr_box){target, 0, length, destination->height};
    } else {
      options.src_box = (struct wlr_fbox){0, start, destination->width, length};
      options.dst_box = (struct wlr_box){0, target, destination->width, length};
    }
    wlr_render_pass_add_texture(pass, &options);
  }
  const bool ok = wlr_render_pass_submit(pass);
  wlr_texture_destroy(texture);
  return ok;
}

// Copies `box` of source scaled to all of destination (bilinear).
static bool ludash_backdrop_copy(struct wlr_renderer *renderer,
                                 struct wlr_buffer *source,
                                 struct wlr_buffer *destination,
                                 struct wlr_fbox box) {
  struct wlr_texture *texture = wlr_texture_from_buffer(renderer, source);
  if (!texture)
    return false;
  struct wlr_render_pass *pass =
      wlr_renderer_begin_buffer_pass(renderer, destination, NULL);
  if (!pass) {
    wlr_texture_destroy(texture);
    return false;
  }
  const struct wlr_render_texture_options options = {
      .texture = texture,
      .src_box = box,
      .dst_box = {0, 0, destination->width, destination->height},
      .filter_mode = WLR_SCALE_FILTER_BILINEAR,
      .blend_mode = WLR_RENDER_BLEND_MODE_NONE};
  wlr_render_pass_add_texture(pass, &options);
  const bool ok = wlr_render_pass_submit(pass);
  wlr_texture_destroy(texture);
  return ok;
}

// The blur's Gaussian sigma in scene pixels per unit of the radius setting:
// radius 18 (the default) is a clearly frosted sigma of 18 px.
#define LUDASH_BLUR_SIGMA 1.0
// The 9-tap kernel's own sigma, in taps.
#define LUDASH_KERNEL_SIGMA 1.69
#define LUDASH_BLUR_MAX_ITERATIONS 8

// Scene pixels per working pixel: the blur runs at a reduced resolution.
static double ludash_backdrop_scale(const struct wlr_box *area) {
  return fmax(4.0, fmax((double)area->width / (LUDASH_BACKDROP_SIZE - 20),
                        (double)area->height / (LUDASH_BACKDROP_SIZE - 20)));
}

void ludash_scene_backdrop_sample_box(const struct wlr_box *area, int radius,
                                      struct wlr_box *sample) {
  *sample = *area;
  if (area->width < 1 || area->height < 1)
    return;
  const double scale = ludash_backdrop_scale(area);
  // Three sigmas of blur plus the rounding of the reduced capture.
  const int padding = (int)ceil(3.0 * radius * LUDASH_BLUR_SIGMA + 3.0 * scale);
  sample->x -= padding;
  sample->y -= padding;
  sample->width += 2 * padding;
  sample->height += 2 * padding;
}

struct wlr_buffer *ludash_scene_backdrop_render(
    struct wlr_renderer *renderer, struct wlr_allocator *allocator,
    struct wl_event_loop *loop, struct wlr_scene_node *root,
    struct wlr_scene_node *stop, struct wlr_scene_node *skip,
    const struct wlr_box *area, int radius) {
  if (!renderer || !allocator || !root || !stop || !area || radius < 1 ||
      radius > 32 || area->width < 1 || area->height < 1 ||
      area->width > 16384 || area->height > 16384 || area->x < -65536 ||
      area->x > 65536 || area->y < -65536 || area->y > 65536 || root == stop ||
      skip == stop)
    return NULL;
  const double scale = ludash_backdrop_scale(area);
  const int width = (int)ceil(area->width / scale);
  const int height = (int)ceil(area->height / scale);
  const double scale_x = (double)width / area->width;
  const double scale_y = (double)height / area->height;
  const double sigma_x = radius * LUDASH_BLUR_SIGMA * scale_x;
  const double sigma_y = radius * LUDASH_BLUR_SIGMA * scale_y;
  const int padding_x = (int)ceil(3.0 * sigma_x) + 2;
  const int padding_y = (int)ceil(3.0 * sigma_y) + 2;
  const int work_width = width + 2 * padding_x;
  const int work_height = height + 2 * padding_y;

  // Captured at twice the working size, then halved: every working pixel
  // averages four, instead of the capture skipping most of the scene.
  struct wlr_buffer *capture_buffer =
      ludash_scratch_take(renderer, allocator, 2 * work_width, 2 * work_height);
  struct wlr_buffer *work[2] = {
      ludash_scratch_take(renderer, allocator, work_width, work_height),
      ludash_scratch_take(renderer, allocator, work_width, work_height)};
  struct wlr_buffer *result =
      ludash_backdrop_buffer(renderer, allocator, width, height);
  struct wlr_texture **imports =
      calloc(LUDASH_BACKDROP_LIMIT, sizeof(*imports));
  struct ludash_backdrop_hold *hold = NULL;
  bool ok = false;
  if (!capture_buffer || !work[0] || !work[1] || !result || !imports)
    goto cleanup;

  struct wlr_buffer_pass_options pass_options = {0};
#if LUDASH_BACKDROP_TIMELINES
  // Explicit-sync sources need our completion signalled (see the hold).
  if (loop && renderer->features.timeline) {
    hold = calloc(1, sizeof(*hold));
    if (hold)
      hold->buffers = calloc(LUDASH_BACKDROP_LIMIT, sizeof(*hold->buffers));
    if (hold && hold->buffers)
      hold->timeline =
          wlr_drm_syncobj_timeline_create(wlr_renderer_get_drm_fd(renderer));
    if (hold && hold->timeline) {
      pass_options.signal_timeline = hold->timeline;
      pass_options.signal_point = 1;
    } else if (hold) {
      ludash_backdrop_release(hold);
      hold = NULL;
    }
  }
#else
  (void)loop;
#endif
  struct wlr_render_pass *pass =
      wlr_renderer_begin_buffer_pass(renderer, capture_buffer, &pass_options);
  if (!pass)
    goto cleanup;
  const struct wlr_render_rect_options clear = {
      .box = {0, 0, capture_buffer->width, capture_buffer->height},
      .color = {0.043f, 0.067f, 0.078f, 1.0f},
      .blend_mode = WLR_RENDER_BLEND_MODE_NONE};
  wlr_render_pass_add_rect(pass, &clear);
  struct ludash_backdrop_capture capture = {.renderer = renderer,
                                            .pass = pass,
                                            .stop = stop,
                                            .skip = skip,
                                            .imports = imports,
                                            .hold = hold,
                                            .x = area->x - padding_x / scale_x,
                                            .y = area->y - padding_y / scale_y,
                                            .scale_x = 2.0 * scale_x,
                                            .scale_y = 2.0 * scale_y,
                                            .width = capture_buffer->width,
                                            .height = capture_buffer->height};
  ludash_backdrop_visit(&capture, root, 0, 0, 0);
  const bool submitted = wlr_render_pass_submit(pass);
  for (size_t index = 0; index < capture.import_count; ++index)
    wlr_texture_destroy(imports[index]);
#if LUDASH_BACKDROP_TIMELINES
  // Held client buffers go back once the GPU has read them.
  if (hold && hold->count > 0 && submitted &&
      wlr_drm_syncobj_timeline_waiter_init(&hold->waiter, hold->timeline, 1, 0,
                                           loop, ludash_backdrop_done))
    hold = NULL;
#endif
  if (!submitted || !capture.found || capture.failed)
    goto cleanup;

  ok = ludash_backdrop_copy(
      renderer, capture_buffer, work[0],
      (struct wlr_fbox){0, 0, capture_buffer->width, capture_buffer->height});
  // Repeated small-kernel passes add up to the wanted sigma (variances add)
  // without taps far enough apart to show copies of the scene.
  const double sigma = fmax(sigma_x, sigma_y);
  const double spacing = fmin(1.5, fmax(0.5, sigma / LUDASH_KERNEL_SIGMA));
  const double pass_sigma = LUDASH_KERNEL_SIGMA * spacing;
  int iterations = (int)ceil((sigma * sigma) / (pass_sigma * pass_sigma));
  if (iterations < 1)
    iterations = 1;
  if (iterations > LUDASH_BLUR_MAX_ITERATIONS)
    iterations = LUDASH_BLUR_MAX_ITERATIONS;
  for (int i = 0; ok && i < iterations; ++i)
    ok = ludash_backdrop_blur(renderer, work[0], work[1], spacing, true) &&
         ludash_backdrop_blur(renderer, work[1], work[0], spacing, false);
  if (ok)
    ok = ludash_backdrop_copy(
        renderer, work[0], result,
        (struct wlr_fbox){padding_x, padding_y, width, height});
cleanup:
  if (hold)
    ludash_backdrop_release(hold);
  free(imports);
  ludash_scratch_give(capture_buffer);
  for (int i = 0; i < 2; ++i)
    ludash_scratch_give(work[i]);
  if (!ok && result) {
    wlr_buffer_drop(result);
    result = NULL;
  }
  return result;
}
