#include "compositor/renderer/clip/CornerGeometry.h"
#include <math.h>
#include <stdbool.h>

static bool ludash_corner_append(struct ludash_corner_band *bands,
                                 size_t capacity, size_t *count, int x, int y,
                                 int width, int height) {
  if (height <= 0)
    return true;
  if (*count > 0) {
    struct ludash_corner_band *last = &bands[*count - 1];
    if (last->x == x && last->width == width &&
        last->y + last->height == y) {
      last->height += height;
      return true;
    }
  }
  if (*count >= capacity)
    return false;
  bands[(*count)++] = (struct ludash_corner_band){x, y, width, height};
  return true;
}

static int ludash_corner_inset(int radius, int row) {
  const double dy = radius - row - 0.5;
  const double edge = radius - sqrt((double)radius * radius - dy * dy) - 0.5;
  return edge > 0 ? (int)ceil(edge) : 0;
}

size_t ludash_corner_bands(int width, int height, int radius,
                           struct ludash_corner_band *bands, size_t capacity) {
  if (!bands || capacity == 0 || width <= 0 || height <= 0 ||
      width > 16384 || height > 16384)
    return 0;
  if (radius < 0)
    radius = 0;
  if (radius > 32)
    radius = 32;
  if (radius > width / 2)
    radius = width / 2;
  if (radius > height / 2)
    radius = height / 2;
  size_t count = 0;
  for (int y = 0; y < radius; ++y) {
    const int inset = ludash_corner_inset(radius, y);
    if (!ludash_corner_append(bands, capacity, &count, inset, y,
                              width - inset * 2, 1))
      return 0;
  }
  if (!ludash_corner_append(bands, capacity, &count, 0, radius, width,
                            height - radius * 2))
    return 0;
  for (int y = height - radius; y < height; ++y) {
    const int inset = ludash_corner_inset(radius, height - y - 1);
    if (!ludash_corner_append(bands, capacity, &count, inset, y,
                              width - inset * 2, 1))
      return 0;
  }
  return count;
}
