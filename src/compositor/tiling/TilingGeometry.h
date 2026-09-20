#pragma once
#include <stddef.h>
#ifdef __cplusplus
namespace LunaDash {
extern "C" {
#endif
typedef struct LuDashRectangle {
  int x, y, width, height;
} LuDashRectangle;

/*
 * Places one full-height window in each horizontal column. Column widths are
 * caller-owned state: adding another width never changes an existing column.
 * Rectangles may be outside area horizontally. On failure output is unchanged.
 */
size_t ludash_layout_columns(LuDashRectangle area, const int *widths,
                             size_t count, int gap, int scroll_offset,
                             LuDashRectangle *output, size_t capacity);

/* Up to eight vertical rows. Weights divide height without overlap. */
size_t ludash_layout_weighted_column_windows(LuDashRectangle column,
                                             const int *weights, size_t count,
                                             int gap, LuDashRectangle *output,
                                             size_t capacity);
size_t ludash_layout_column_windows(LuDashRectangle column, size_t count,
                                    int gap, LuDashRectangle *output,
                                    size_t capacity);

/* Compatibility helper using one ratio-derived width for every column. */
size_t ludash_tile_rectangles(LuDashRectangle area, size_t count, double ratio,
                              int gap, LuDashRectangle *output,
                              size_t capacity);
#ifdef __cplusplus
}
}
#endif
