#include "compositor/renderer/clip/CornerGeometry.h"

#include "support/TestHarness.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <limits>

namespace LunaDash {
namespace {
int effectiveRadius(int width, int height, int radius) {
  return std::max(0, std::min({radius, 32, width / 2, height / 2}));
}

bool insideRoundedRectangle(int x, int y, int width, int height, int radius) {
  const double px = x + 0.5;
  const double py = y + 0.5;
  const double nearestX = std::clamp(px, double(radius), double(width - radius));
  const double nearestY = std::clamp(py, double(radius), double(height - radius));
  const double dx = px - nearestX;
  const double dy = py - nearestY;
  return dx * dx + dy * dy <= double(radius) * radius;
}
} // namespace

class CornerGeometryTests final {
public:
  struct CoverageRow {
    const char *name;
    int width;
    int height;
    int requestedRadius;
  };

  void coverage() {
    static const CoverageRow rows[] = {
        {"normal", 96, 72, 16},
        {"portrait", 37, 81, 16},
        {"odd-square", 33, 33, 16},
        {"circle", 32, 32, 16},
        {"small", 7, 5, 16},
        {"single-pixel", 1, 1, 16},
        {"single-column", 1, 37, 16},
        {"single-row", 37, 1, 16},
        {"square-corners", 37, 23, 0},
        {"negative-radius", 37, 23, -1},
        {"maximum-radius", 96, 80, 32},
        {"clamped-radius", 96, 80, std::numeric_limits<int>::max()},
    };
    for (const auto &row : rows)
      coverageFor(row.width, row.height, row.requestedRadius);
  }

  void coverageFor(int width, int height, int requestedRadius) {
    std::array<ludash_corner_band, LUDASH_CORNER_MAX_BANDS> bands{};
    const auto count = ludash_corner_bands(width, height, requestedRadius,
                                           bands.data(), bands.size());
    VERIFY(count > 0);
    VERIFY(count <= bands.size());
    std::vector<int> coverage(width * height, 0);
    int nextY = 0;
    for (std::size_t i = 0; i < count; ++i) {
      const auto &band = bands[i];
      VERIFY(band.x >= 0);
      VERIFY(band.width > 0);
      VERIFY(band.height > 0);
      COMPARE(band.y, nextY);
      VERIFY(band.x + band.width <= width);
      VERIFY(band.y + band.height <= height);
      COMPARE(band.x * 2 + band.width, width);
      if (i > 0)
        VERIFY(bands[i - 1].x != band.x || bands[i - 1].width != band.width);
      nextY += band.height;
      for (int y = band.y; y < band.y + band.height; ++y)
        for (int x = band.x; x < band.x + band.width; ++x)
          ++coverage[y * width + x];
    }
    COMPARE(nextY, height);
    const int radius = effectiveRadius(width, height, requestedRadius);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const int actual = coverage[y * width + x];
        const int expected = insideRoundedRectangle(x, y, width, height, radius)
                                 ? 1 : 0;
        COMPARE(actual, expected);
        COMPARE(actual, coverage[y * width + width - x - 1]);
        COMPARE(actual, coverage[(height - y - 1) * width + x]);
      }
    }
  }

  void capacityAndInvalidGeometry() {
    std::array<ludash_corner_band, LUDASH_CORNER_MAX_BANDS> bands{};
    const auto count = ludash_corner_bands(100, 80, 16, bands.data(), bands.size());
    VERIFY(count > 1);
    COMPARE(ludash_corner_bands(100, 80, 16, bands.data(), count), count);
    COMPARE(ludash_corner_bands(100, 80, 16, bands.data(), count - 1), std::size_t(0));
    COMPARE(ludash_corner_bands(100, 80, 16, nullptr, bands.size()), std::size_t(0));
    COMPARE(ludash_corner_bands(100, 80, 16, bands.data(), 0), std::size_t(0));
    for (const auto invalid : {0, -1, 16385, std::numeric_limits<int>::max()}) {
      COMPARE(ludash_corner_bands(invalid, 80, 16, bands.data(), bands.size()),
               std::size_t(0));
      COMPARE(ludash_corner_bands(100, invalid, 16, bands.data(), bands.size()),
               std::size_t(0));
    }
  }

  void boundedWorkForLargeWindows() {
    std::array<ludash_corner_band, LUDASH_CORNER_MAX_BANDS> bands{};
    const auto count = ludash_corner_bands(16384, 16384, 1000000,
                                           bands.data(), bands.size());
    VERIFY(count > 1);
    VERIFY(count <= bands.size());
    int nextY = 0;
    bool hasFullWidthCenter = false;
    for (std::size_t i = 0; i < count; ++i) {
      const auto &band = bands[i];
      COMPARE(band.y, nextY);
      VERIFY(band.x >= 0 && band.x < 32);
      COMPARE(band.x * 2 + band.width, 16384);
      VERIFY(band.height > 0);
      nextY += band.height;
      if (band.x == 0 && band.height >= 16384 - 64)
        hasFullWidthCenter = true;
    }
    COMPARE(nextY, 16384);
    VERIFY(hasFullWidthCenter);
  }
};
} // namespace LunaDash

int main(int argc, char **argv) {
  return LunaDash::Test::run<LunaDash::CornerGeometryTests>(
      "CornerGeometryTests",
      {
      {"coverage", &LunaDash::CornerGeometryTests::coverage},
      {"capacityAndInvalidGeometry", &LunaDash::CornerGeometryTests::capacityAndInvalidGeometry},
      {"boundedWorkForLargeWindows", &LunaDash::CornerGeometryTests::boundedWorkForLargeWindows}},
      argc, argv);
}
