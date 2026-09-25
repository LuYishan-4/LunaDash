#include "core/templates/WindowAnimation.hpp"
#include <QtTest>

namespace LunaDash {
namespace {
struct FakeAnimationBackend {
  int configured = 0;
  int opened = 0;
  int closed = 0;
  int relayouts = 0;
  int focused = 0;
  int cancelled = 0;
  int clears = 0;
  int advances = 0;

  void configure(int value) { configured = value; }
  void open(int value) { opened = value; }
  void close(int value) { closed = value; }
  void relayout(int value, int delta) { relayouts = value + delta; }
  void focus(int value) { focused = value; }
  void cancel(int value) { cancelled = value; }
  void clear() { ++clears; }
  void advance() { ++advances; }
  int activeCount() const { return opened != closed ? 1 : 0; }
};
} // namespace

class WindowAnimationTemplateTests final : public QObject {
  Q_OBJECT

private slots:
  void forwardsWindowLifecycle() {
    Templates::WindowAnimationTemplate<FakeAnimationBackend> animation;
    animation.configure(7);
    animation.open(11);
    animation.relayout(10, 5);
    animation.focus(12);
    animation.cancel(13);

    QCOMPARE(animation.backend().configured, 7);
    QCOMPARE(animation.backend().opened, 11);
    QCOMPARE(animation.backend().relayouts, 15);
    QCOMPARE(animation.backend().focused, 12);
    QCOMPARE(animation.backend().cancelled, 13);
    QCOMPARE(animation.activeCount(), 1);

    animation.close(11);
    QCOMPARE(animation.activeCount(), 0);
    animation.advance();
    animation.clear();
    QCOMPARE(animation.backend().advances, 1);
    QCOMPARE(animation.backend().clears, 1);
  }
};

} // namespace LunaDash

QTEST_GUILESS_MAIN(LunaDash::WindowAnimationTemplateTests)
#include "WindowAnimationTemplateTests.moc"
