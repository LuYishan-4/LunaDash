#include "compositor/plugins/FadePlugin/FadePlugin.hpp"
#include <QQuickItem>
#include <QPropertyAnimation>
namespace LuDash {
void FadePlugin::windowOpened(QQuickItem* frame) {
    frame->setOpacity(0);
    auto* animation = new QPropertyAnimation(frame, "opacity", frame);
    animation->setDuration(180); animation->setStartValue(0.0); animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
void FadePlugin::windowFocused(QQuickItem*) {}
}
