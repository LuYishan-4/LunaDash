#pragma once
#include <stddef.h>
#ifdef __cplusplus
namespace LunaDash {
extern "C" {
#endif
typedef struct LuDashRectangle {
  int x, y, width, height;
} LuDashRectangle;

/* Split a work area into two disjoint rectangles; preserve child minimums.
 * ratio is in millionths. vertical divides left/right, otherwise top/bottom.
 * Returns zero without changing output when the area cannot contain both. */
int ludash_split_rectangle(LuDashRectangle area, int vertical, int ratio,
                           int gap, int first_minimum, int second_minimum,
                           LuDashRectangle output[2]);

/* Up to eight vertical rows. Weights divide height without overlap. */
size_t ludash_layout_weighted_column_windows(LuDashRectangle column,
                                             const int *weights, size_t count,
                                             int gap, LuDashRectangle *output,
                                             size_t capacity);
size_t ludash_layout_column_windows(LuDashRectangle column, size_t count,
                                    int gap, LuDashRectangle *output,
                                    size_t capacity);

#ifdef __cplusplus
}
}
#endif
