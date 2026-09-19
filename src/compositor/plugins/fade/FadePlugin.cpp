#include "compositor/plugins/fade/FadePlugin.hpp"
#include <QPropertyAnimation>
#include <QQuickItem>
namespace LunaDash {
void FadePlugin::windowOpened(QQuickItem *frame) {
  frame->setOpacity(0);
  auto *animation = new QPropertyAnimation(frame, "opacity", frame);
  animation->setDuration(180);
  animation->setStartValue(0.0);
  animation->setEndValue(1.0);
  animation->start(QAbstractAnimation::DeleteWhenStopped);
}
void FadePlugin::windowFocused(QQuickItem *) {}
} // namespace LunaDash
