#pragma once
#include <stdbool.h>
struct wlr_output;
struct wlr_scene_output;
#ifdef __cplusplus
namespace LunaDash {
extern "C" {
#endif
struct ludash_night_color;
struct ludash_night_color *ludash_night_color_create(int kelvin);
void ludash_night_color_destroy(struct ludash_night_color *color);
bool ludash_night_color_apply(struct wlr_output *output,
                              struct ludash_night_color *color);
bool ludash_night_color_commit(struct wlr_scene_output *output,
                               struct ludash_night_color *color);
#ifdef __cplusplus
}
}
#endif
