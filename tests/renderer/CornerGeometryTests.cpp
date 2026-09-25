#include "compositor/renderer/clip/CornerGeometry.h"

#include <QtTest>
#include <QVector>
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

class CornerGeometryTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void coverage_data() {
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<int>("requestedRadius");
    QTest::newRow("normal") << 96 << 72 << 16;
    QTest::newRow("portrait") << 37 << 81 << 16;
    QTest::newRow("odd-square") << 33 << 33 << 16;
    QTest::newRow("circle") << 32 << 32 << 16;
    QTest::newRow("small") << 7 << 5 << 16;
    QTest::newRow("single-pixel") << 1 << 1 << 16;
    QTest::newRow("single-column") << 1 << 37 << 16;
    QTest::newRow("single-row") << 37 << 1 << 16;
    QTest::newRow("square-corners") << 37 << 23 << 0;
    QTest::newRow("negative-radius") << 37 << 23 << -1;
    QTest::newRow("maximum-radius") << 96 << 80 << 32;
    QTest::newRow("clamped-radius") << 96 << 80
                                      << std::numeric_limits<int>::max();
  }

  void coverage() {
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(int, requestedRadius);
    std::array<ludash_corner_band, LUDASH_CORNER_MAX_BANDS> bands{};
    const auto count = ludash_corner_bands(width, height, requestedRadius,
                                           bands.data(), bands.size());
    QVERIFY(count > 0);
    QVERIFY(count <= bands.size());
    QVector<int> coverage(width * height, 0);
    int nextY = 0;
    for (std::size_t i = 0; i < count; ++i) {
      const auto &band = bands[i];
      QVERIFY(band.x >= 0);
      QVERIFY(band.width > 0);
      QVERIFY(band.height > 0);
      QCOMPARE(band.y, nextY);
      QVERIFY(band.x + band.width <= width);
      QVERIFY(band.y + band.height <= height);
      QCOMPARE(band.x * 2 + band.width, width);
      if (i > 0)
        QVERIFY(bands[i - 1].x != band.x || bands[i - 1].width != band.width);
      nextY += band.height;
      for (int y = band.y; y < band.y + band.height; ++y)
        for (int x = band.x; x < band.x + band.width; ++x)
          ++coverage[y * width + x];
    }
    QCOMPARE(nextY, height);
    const int radius = effectiveRadius(width, height, requestedRadius);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const int actual = coverage[y * width + x];
        const int expected = insideRoundedRectangle(x, y, width, height, radius)
                                 ? 1 : 0;
        QCOMPARE(actual, expected);
        QCOMPARE(actual, coverage[y * width + width - x - 1]);
        QCOMPARE(actual, coverage[(height - y - 1) * width + x]);
      }
    }
  }

  void capacityAndInvalidGeometry() {
    std::array<ludash_corner_band, LUDASH_CORNER_MAX_BANDS> bands{};
    const auto count = ludash_corner_bands(100, 80, 16, bands.data(), bands.size());
    QVERIFY(count > 1);
    QCOMPARE(ludash_corner_bands(100, 80, 16, bands.data(), count), count);
    QCOMPARE(ludash_corner_bands(100, 80, 16, bands.data(), count - 1), std::size_t(0));
    QCOMPARE(ludash_corner_bands(100, 80, 16, nullptr, bands.size()), std::size_t(0));
    QCOMPARE(ludash_corner_bands(100, 80, 16, bands.data(), 0), std::size_t(0));
    for (const auto invalid : {0, -1, 16385, std::numeric_limits<int>::max()}) {
      QCOMPARE(ludash_corner_bands(invalid, 80, 16, bands.data(), bands.size()),
               std::size_t(0));
      QCOMPARE(ludash_corner_bands(100, invalid, 16, bands.data(), bands.size()),
               std::size_t(0));
    }
  }

  void boundedWorkForLargeWindows() {
    std::array<ludash_corner_band, LUDASH_CORNER_MAX_BANDS> bands{};
    const auto count = ludash_corner_bands(16384, 16384, 1000000,
                                           bands.data(), bands.size());
    QVERIFY(count > 1);
    QVERIFY(count <= bands.size());
    int nextY = 0;
    bool hasFullWidthCenter = false;
    for (std::size_t i = 0; i < count; ++i) {
      const auto &band = bands[i];
      QCOMPARE(band.y, nextY);
      QVERIFY(band.x >= 0 && band.x < 32);
      QCOMPARE(band.x * 2 + band.width, 16384);
      QVERIFY(band.height > 0);
      nextY += band.height;
      if (band.x == 0 && band.height >= 16384 - 64)
        hasFullWidthCenter = true;
    }
    QCOMPARE(nextY, 16384);
    QVERIFY(hasFullWidthCenter);
  }
};
} // namespace LunaDash

QTEST_GUILESS_MAIN(LunaDash::CornerGeometryTests)
#include "CornerGeometryTests.moc"
