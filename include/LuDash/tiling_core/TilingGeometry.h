#pragma once
#include <stddef.h>
#ifdef __cplusplus
namespace LuDash {
extern "C" {
#endif
typedef struct LuDashRectangle { int x, y, width, height; } LuDashRectangle;
size_t ludash_tile_rectangles(LuDashRectangle area, size_t count, double ratio, int gap, LuDashRectangle* output, size_t capacity);
#ifdef __cplusplus
}
}
#endif
