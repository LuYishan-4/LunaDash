#pragma once

#include <stddef.h>

#define LUDASH_CORNER_MAX_BANDS 65

#ifdef __cplusplus
namespace LunaDash {
extern "C" {
#endif

struct ludash_corner_band {
  int x, y, width, height;
};

/* Disjoint pixel-center coverage of a rounded rectangle, merged into horizontal
 * bands. Radius is bounded to 32 logical pixels and half the window size.
 * Invalid dimensions, null output or insufficient capacity return zero. */
size_t ludash_corner_bands(int width, int height, int radius,
                           struct ludash_corner_band *bands, size_t capacity);

#ifdef __cplusplus
}
}
#endif
