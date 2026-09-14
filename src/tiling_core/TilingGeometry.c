#include <LuDash/tiling_core/TilingGeometry.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define LUDASH_MAX_COLUMNS 4096U

static int ludash_valid_area(LuDashRectangle area) {
  return area.width > 0 && area.height > 0 && area.x >= 0 && area.y >= 0 &&
         area.x <= INT_MAX - area.width && area.y <= INT_MAX - area.height;
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

size_t ludash_layout_column_windows(LuDashRectangle column, size_t count,
                                    int gap, LuDashRectangle *output,
                                    size_t capacity) {
  if (!output || count == 0 || count > capacity || count > LUDASH_MAX_COLUMNS ||
      !ludash_valid_area(column) || gap < 0)
    return 0;

  const int64_t gaps = (int64_t)gap * (int64_t)(count - 1);
  if (gaps >= column.height)
    return 0;
  const int available = column.height - (int)gaps;
  const int base_height = available / (int)count;
  const int remainder = available % (int)count;
  if (base_height <= 0)
    return 0;

  int64_t y = column.y;
  for (size_t i = 0; i < count; ++i) {
    const int height = base_height + ((int)i < remainder ? 1 : 0);
    output[i] = (LuDashRectangle){column.x, (int)y, column.width, height};
    y += (int64_t)height + gap;
  }
  return count;
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
