#include <LuDash/blur/BlurGeometry.h>
#include <QtTest>
#include <limits>
namespace LuDash {
class BlurGeometryTests final : public QObject {
    Q_OBJECT
private slots:
    void scaleRoundAndClip() {
        QCOMPARE(blurViewportRegion({10.25, 20.25, 30.5, 40.5}, 2, {200, 200}), QRect(20, 78, 62, 82));
        QCOMPARE(blurViewportRegion({-10, -20, 40, 50}, 1, {100, 100}), QRect(0, 70, 30, 30));
        QVERIFY(blurViewportRegion({200, 200, 10, 10}, 1, {100, 100}).isEmpty());
    }
    void nonFiniteCoordinatesAndInvalidScale() {
        const auto infinity = std::numeric_limits<qreal>::infinity();
        const auto nan = std::numeric_limits<qreal>::quiet_NaN();
        QVERIFY(blurViewportRegion({-infinity, 0, infinity, 10}, 1, {100, 100}).isEmpty());
        QVERIFY(blurViewportRegion({nan, 0, 20, 20}, 1, {100, 100}).isEmpty());
        for (const auto ratio : {qreal(0), qreal(-1), infinity, nan})
            QVERIFY(blurViewportRegion({0, 0, 10, 10}, ratio, {100, 100}).isEmpty());
        QVERIFY(blurViewportRegion({0, 0, 10, 10}, 1, {}).isEmpty());
    }
    void hugeCoordinatesStayBounded() {
        QCOMPARE(blurViewportRegion({-1e100, -1e100, 2e100, 2e100}, 2, {1920, 1080}), QRect(0, 0, 1920, 1080));
        QVERIFY(blurViewportRegion({1e100, 1e100, 10, 10}, 1, {1920, 1080}).isEmpty());
    }
};
}
QTEST_APPLESS_MAIN(LuDash::BlurGeometryTests)
#include "BlurGeometryTests.moc"
