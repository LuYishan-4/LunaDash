#include "compositor/tiling/TilingGeometry.h"

#include <limits.h>
#include <stdint.h>

static int ludash_valid_area(LuDashRectangle area) {
  return area.width > 0 && area.height > 0 && area.x <= INT_MAX - area.width &&
         area.y <= INT_MAX - area.height;
}

int ludash_split_rectangle(LuDashRectangle area, int vertical, int ratio,
                           int gap, int first_minimum, int second_minimum,
                           LuDashRectangle output[2]) {
  const int length = vertical ? area.width : area.height;
  if (!output || !ludash_valid_area(area) || gap < 0 || ratio < 0 ||
      ratio > 1000000 || first_minimum < 1 || second_minimum < 1 ||
      (int64_t)first_minimum + second_minimum > length)
    return 0;
  const int maximum_gap = length - first_minimum - second_minimum;
  if (gap > maximum_gap)
    gap = maximum_gap;
  const int available = length - gap;
  int first = (int)(((int64_t)available * ratio + 500000) / 1000000);
  if (first < first_minimum)
    first = first_minimum;
  if (first > available - second_minimum)
    first = available - second_minimum;
  output[0] = output[1] = area;
  if (vertical) {
    output[0].width = first;
    output[1].x += first + gap;
    output[1].width = available - first;
  } else {
    output[0].height = first;
    output[1].y += first + gap;
    output[1].height = available - first;
  }
  return 1;
}

size_t ludash_layout_weighted_column_windows(LuDashRectangle column,
                                             const int *weights, size_t count,
                                             int gap, LuDashRectangle *output,
                                             size_t capacity) {
  if (!output || !weights || count == 0 || count > capacity || count > 8 ||
      !ludash_valid_area(column) || gap < 0 || column.height < (int)count)
    return 0;
  int64_t total = 0;
  for (size_t i = 0; i < count; ++i) {
    if (weights[i] <= 0 || weights[i] > INT_MAX / 8)
      return 0;
    total += weights[i];
  }
  if (count > 1 && gap > (column.height - (int)count) / (int)(count - 1))
    gap = (column.height - (int)count) / (int)(count - 1);
  const int available = column.height - gap * (int)(count - 1);
  const int extra = available - (int)count;
  int64_t prefix = 0;
  int used = 0;
  int y = column.y;
  for (size_t i = 0; i < count; ++i) {
    prefix += weights[i];
    const int boundary = (int)((int64_t)extra * prefix / total);
    const int height = 1 + boundary - used;
    output[i] = (LuDashRectangle){column.x, y, column.width, height};
    used = boundary;
    y += height + (i + 1 < count ? gap : 0);
  }
  return count;
}

size_t ludash_layout_column_windows(LuDashRectangle column, size_t count,
                                    int gap, LuDashRectangle *output,
                                    size_t capacity) {
  const int weights[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  return ludash_layout_weighted_column_windows(column, weights, count, gap,
                                               output, capacity);
}
