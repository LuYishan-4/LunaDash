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
  if (!output || count == 0 || count > capacity || count > 4 ||
      !ludash_valid_area(column) || gap < 0)
    return 0;
  if (count == 1) {
    output[0] = column;
    return 1;
  }
  if (gap >= column.width)
    return 0;
  const int left_width = (column.width - gap) / 2;
  const int right_width = column.width - left_width - gap;
  if (left_width <= 0 || right_width <= 0)
    return 0;
  const int right_x = column.x + left_width + gap;
  if (count == 2) {
    output[0] =
        (LuDashRectangle){column.x, column.y, left_width, column.height};
    output[1] =
        (LuDashRectangle){right_x, column.y, right_width, column.height};
    return 2;
  }
  if (gap >= column.height)
    return 0;
  const int top_height = (column.height - gap) / 2;
  const int bottom_height = column.height - top_height - gap;
  if (top_height <= 0 || bottom_height <= 0)
    return 0;
  const int bottom_y = column.y + top_height + gap;
  if (count == 3) {
    output[0] =
        (LuDashRectangle){column.x, column.y, left_width, column.height};
    output[1] = (LuDashRectangle){right_x, column.y, right_width, top_height};
    output[2] =
        (LuDashRectangle){right_x, bottom_y, right_width, bottom_height};
    return 3;
  }
  output[0] = (LuDashRectangle){column.x, column.y, left_width, top_height};
  output[1] = (LuDashRectangle){right_x, column.y, right_width, top_height};
  output[2] = (LuDashRectangle){column.x, bottom_y, left_width, bottom_height};
  output[3] = (LuDashRectangle){right_x, bottom_y, right_width, bottom_height};
  return 4;
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
