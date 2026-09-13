#include <LuDash/animation/WindowAnimations.h>
#include <QtTest>
#include <QQuickWindow>
namespace LuDash {
class AnimationTests final : public QObject {
    Q_OBJECT
private slots:
    void cancellationAndDestruction() {
        WindowAnimations animations;
        animations.setDuration(100);
        auto* item = new QQuickItem;
        animations.show(item); QCOMPARE(animations.activeCount(), 1);
        animations.hide(item); QCOMPARE(animations.activeCount(), 1);
        animations.show(item); QCOMPARE(animations.activeCount(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(animations.activeCount(), 0, 1000);
        QVERIFY(item->isVisible()); QCOMPARE(item->scale(), 1.0);
        animations.hide(item); delete item;
        QCOMPARE(animations.activeCount(), 0);
    }
    void reducedMotionCompletesCallbacks() {
        WindowAnimations animations;
        QQuickWindow window; window.show();
        QQuickItem item(window.contentItem());
        bool finished = false;
        animations.show(&item);
        animations.hide(&item, [&] { finished = true; });
        animations.setDuration(0);
        QVERIFY(finished); QVERIFY(!item.isVisible()); QCOMPARE(animations.activeCount(), 0);
        animations.show(&item); QVERIFY(item.isVisible()); QCOMPARE(animations.activeCount(), 0);
    }
    void cancelledGroupCannotRemoveReplacement() {
        WindowAnimations animations;
        animations.setDuration(600);
        QQuickItem item;
        bool cancelledCallback = false;
        animations.hide(&item, [&] { cancelledCallback = true; });
        animations.show(&item);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(animations.activeCount(), 1);
        QVERIFY(!cancelledCallback);
        animations.setDuration(0);
        QCOMPARE(animations.activeCount(), 0);
        QVERIFY(item.isVisible());
    }
    void controllerCanBeDestroyedBeforeItem() {
        auto* item = new QQuickItem;
        bool finished = false;
        {
            WindowAnimations animations;
            animations.hide(item, [&] { finished = true; });
        }
        delete item;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!finished);
    }
    void completionCanDestroyItems() {
        WindowAnimations animations;
        auto* first = new QQuickItem;
        auto* second = new QQuickItem;
        animations.show(second);
        animations.hide(first, [&] { delete first; delete second; });
        animations.setDuration(0);
        QCOMPARE(animations.activeCount(), 0);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};
}
QTEST_MAIN(LuDash::AnimationTests)
#include "AnimationTests.moc"
