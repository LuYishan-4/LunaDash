#include "compositor/tiling/TilingGeometry.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define LUDASH_MAX_COLUMNS 4096U

static int ludash_valid_area(LuDashRectangle area) {
  // Off-screen columns legitimately have negative x/y when the strip is
  // scrolled; only reject empty sizes and upper overflow, not negative
  // coordinates. Lower overflow is guarded by ludash_layout_columns.
  return area.width > 0 && area.height > 0 && area.x <= INT_MAX - area.width &&
         area.y <= INT_MAX - area.height;
}

size_t ludash_layout_columns(LuDashRectangle area, const int *widths,
                             size_t count, int gap, int scroll_offset,
                             LuDashRectangle *output, size_t capacity) {
  if (!output || !widths || count == 0 || count > capacity ||
      count > LUDASH_MAX_COLUMNS || !ludash_valid_area(area) || gap < 0 ||
      scroll_offset < 0) {
    return 0;
  }

  int64_t x = (int64_t)area.x - scroll_offset;
  for (size_t i = 0; i < count; ++i) {
    if (widths[i] <= 0 || x < INT_MIN || x > INT_MAX ||
        x + widths[i] < INT_MIN || x + widths[i] > INT_MAX) {
      return 0;
    }
    if (i + 1 < count) {
      if (x > INT64_MAX - widths[i] - gap)
        return 0;
      x += (int64_t)widths[i] + gap;
    }
  }

  x = (int64_t)area.x - scroll_offset;
  for (size_t i = 0; i < count; ++i) {
    output[i] = (LuDashRectangle){(int)x, area.y, widths[i], area.height};
    x += (int64_t)widths[i] + gap;
  }
  return count;
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

size_t ludash_tile_rectangles(LuDashRectangle area, size_t count, double ratio,
                              int gap, LuDashRectangle *output,
                              size_t capacity) {
  if (!output || count == 0 || count > capacity || count > LUDASH_MAX_COLUMNS ||
      !ludash_valid_area(area) || !isfinite(ratio)) {
    return 0;
  }
  if (ratio < 0.1)
    ratio = 0.1;
  if (ratio > 1.0)
    ratio = 1.0;
  if (gap < 0)
    gap = 0;

  int width = (int)((double)area.width * ratio);
  if (width < 1)
    width = 1;

  int *widths = (int *)malloc(count * sizeof(*widths));
  if (!widths)
    return 0;
  for (size_t i = 0; i < count; ++i)
    widths[i] = width;
  const size_t result =
      ludash_layout_columns(area, widths, count, gap, 0, output, capacity);
  free(widths);
  return result;
}
