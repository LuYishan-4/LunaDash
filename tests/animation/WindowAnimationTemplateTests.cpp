#include "core/templates/WindowAnimation.hpp"
#include "support/TestHarness.hpp"

namespace LunaDash {
namespace {
struct TestAnimation {
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

class WindowAnimationTemplateTests final {
public:
  void forwardsWindowLifecycle() {
    Templates::WindowAnimationTemplate<TestAnimation> animation;
    animation.configure(7);
    animation.open(11);
    animation.relayout(10, 5);
    animation.focus(12);
    animation.cancel(13);

    COMPARE(animation.animation().configured, 7);
    COMPARE(animation.animation().opened, 11);
    COMPARE(animation.animation().relayouts, 15);
    COMPARE(animation.animation().focused, 12);
    COMPARE(animation.animation().cancelled, 13);
    COMPARE(animation.activeCount(), 1);

    animation.close(11);
    COMPARE(animation.activeCount(), 0);
    animation.advance();
    animation.clear();
    COMPARE(animation.animation().advances, 1);
    COMPARE(animation.animation().clears, 1);
  }
};

} // namespace LunaDash

int main(int argc, char **argv) {
  return LunaDash::Test::run<LunaDash::WindowAnimationTemplateTests>(
      "WindowAnimationTemplateTests",
      {
      {"forwardsWindowLifecycle", &LunaDash::WindowAnimationTemplateTests::forwardsWindowLifecycle}},
      argc, argv);
}
