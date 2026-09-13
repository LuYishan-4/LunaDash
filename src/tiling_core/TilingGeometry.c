#include <LuDash/tiling_core/TilingGeometry.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
size_t ludash_tile_rectangles(LuDashRectangle area, size_t count, double ratio, int gap, LuDashRectangle* output, size_t capacity) {
    if (!output || !count || count > capacity || count > 4096 || area.width <= 0 || area.height <= 0 ||
        area.x < 0 || area.y < 0 || area.x > INT_MAX - area.width || area.y > INT_MAX - area.height || !isfinite(ratio)) return 0;
    if (count == 1) { output[0] = area; return 1; }
    if (area.width < 2 || count - 1 > (size_t)area.height) return 0;
    if (ratio < .1) ratio = .1;
    if (ratio > .9) ratio = .9;
    if (gap < 0) gap = 0;
    int max_gap = area.width - 2;
    if (count > 2) {
        const int vertical_gap = (area.height - (int)count + 1) / ((int)count - 2);
        if (vertical_gap < max_gap) max_gap = vertical_gap;
    }
    if (gap > max_gap) gap = max_gap;
    int master = (int)((double)(area.width - gap) * ratio);
    if (master < 1) master = 1;
    if (master > area.width - gap - 1) master = area.width - gap - 1;
    output[0] = (LuDashRectangle){area.x, area.y, master, area.height};
    const int stack_count = (int)count - 1;
    const int stack_height = area.height - gap * (stack_count - 1);
    for (int i = 0; i < stack_count; ++i) {
        const int begin = (int)((int64_t)i * stack_height / stack_count);
        const int end = (int)((int64_t)(i + 1) * stack_height / stack_count);
        output[i + 1] = (LuDashRectangle){area.x + master + gap, area.y + begin + gap * i, area.width - master - gap, end - begin};
    }
    return count;
}
