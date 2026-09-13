#include <LuDash/animation/WindowAnimations.h>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <algorithm>
namespace LuDash {
WindowAnimations::WindowAnimations(QObject* parent) : QObject(parent) {}
WindowAnimations::~WindowAnimations() {
    const auto items = active_.keys();
    for (auto* item : items) cancel(item);
}
void WindowAnimations::setDuration(int milliseconds) {
    duration_ = std::clamp(milliseconds, 0, 600);
    if (!duration_) {
        const auto items = active_.keys();
        for (auto* item : items) {
            // A completion callback may destroy another item or start an animation.
            if (auto* group = active_.value(item)) group->setCurrentTime(group->duration());
        }
    }
}
int WindowAnimations::activeCount() const { return static_cast<int>(active_.size()); }
void WindowAnimations::cancel(QQuickItem* item) {
    if (auto* group = active_.take(item)) {
        disconnect(group, nullptr, this, nullptr);
        group->stop();
        group->deleteLater();
    }
}
void WindowAnimations::show(QQuickItem* item) { animate(item, true, {}); }
void WindowAnimations::hide(QQuickItem* item, const std::function<void()>& finished) { animate(item, false, finished); }
void WindowAnimations::animate(QQuickItem* item, bool showing, const std::function<void()>& finished) {
    const bool alreadyAnimating = active_.contains(item);
    cancel(item);
    if (!duration_) { item->setOpacity(showing ? .999 : 0); item->setScale(1); item->setVisible(showing); if (finished) finished(); return; }
    if (showing && !alreadyAnimating) { item->setOpacity(0); item->setScale(.94); }
    item->setVisible(true); item->setTransformOrigin(QQuickItem::Center);
    auto* group = new QParallelAnimationGroup(item);
    for (const auto& property : {QByteArray("opacity"), QByteArray("scale")}) {
        auto* animation = new QPropertyAnimation(item, property, group);
        animation->setStartValue(item->property(property.constData()));
        animation->setEndValue(property == "opacity" ? (showing ? .999 : 0.0) : (showing ? 1.0 : .94));
        animation->setDuration(duration_); animation->setEasingCurve(showing ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
        group->addAnimation(animation);
    }
    active_.insert(item, group);
    connect(group, &QParallelAnimationGroup::finished, this, [this, item, group, showing, finished] {
        if (active_.value(item) != group) return;
        active_.remove(item);
        group->deleteLater();
        if (!showing) item->setVisible(false);
        if (finished) finished();
    });
    connect(group, &QObject::destroyed, this, [this, item, group] {
        if (active_.value(item) == group) active_.remove(item);
    });
    group->start();
}
}
