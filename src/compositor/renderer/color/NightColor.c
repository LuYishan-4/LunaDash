#include "compositor/renderer/color/NightColor.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/version.h>
#if WLR_VERSION_MINOR >= 20
#include <wlr/render/color.h>
#endif

struct ludash_night_color {
  float green;
  float blue;
#if WLR_VERSION_MINOR >= 20
  struct wlr_color_transform *transform;
#endif
};

struct ludash_night_color *ludash_night_color_create(int kelvin) {
  if (kelvin < 2500 || kelvin > 6500)
    return NULL;
  struct ludash_night_color *color = calloc(1, sizeof(*color));
  if (!color)
    return NULL;
  const double temperature = kelvin / 100.0;
  color->green = (float)fmin(
      1.0, (99.4708025861 * log(temperature) - 161.1195681661) / 255.0);
  color->blue = (float)fmin(
      1.0, (138.5177312231 * log(temperature - 10.0) - 305.0447927307) / 255.0);
  if (kelvin == 6500)
    color->green = color->blue = 1.0f;
#if WLR_VERSION_MINOR >= 20
  const float matrix[9] = {1, 0, 0, 0, color->green, 0, 0, 0, color->blue};
  color->transform = wlr_color_transform_init_matrix(matrix);
  if (!color->transform) {
    free(color);
    return NULL;
  }
#endif
  return color;
}

void ludash_night_color_destroy(struct ludash_night_color *color) {
  if (!color)
    return;
#if WLR_VERSION_MINOR >= 20
  wlr_color_transform_unref(color->transform);
#endif
  free(color);
}

bool ludash_night_color_apply(struct wlr_output *output,
                              struct ludash_night_color *color) {
#if WLR_VERSION_MINOR >= 20
  (void)color;
  return output->image_description == NULL;
#else
  const size_t size = wlr_output_get_gamma_size(output);
  if (size < 2 || size > 65536)
    return color == NULL;
  uint16_t *ramps = calloc(size * 3, sizeof(*ramps));
  if (!ramps)
    return false;
  for (size_t i = 0; i < size; ++i) {
    const double value = 65535.0 * (double)i / (double)(size - 1);
    ramps[i] = (uint16_t)value;
    ramps[size + i] = (uint16_t)(value * (color ? color->green : 1.0));
    ramps[size * 2 + i] = (uint16_t)(value * (color ? color->blue : 1.0));
  }
  struct wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_gamma_lut(&state, size, ramps, ramps + size,
                                 ramps + size * 2);
  const bool ok = wlr_output_test_state(output, &state) &&
                  wlr_output_commit_state(output, &state);
  wlr_output_state_finish(&state);
  free(ramps);
  return ok;
#endif
}

bool ludash_night_color_commit(struct wlr_scene_output *output,
                               struct ludash_night_color *color) {
#if WLR_VERSION_MINOR >= 20
  struct wlr_scene_output_state_options options = {0};
  options.color_transform = color ? color->transform : NULL;
  return wlr_scene_output_commit(output, &options);
#else
  (void)color;
  return wlr_scene_output_commit(output, NULL);
#endif
}
