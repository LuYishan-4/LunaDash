#pragma once
#include <QObject>
#include <QQuickItem>
#include <QHash>
#include <QPointer>
#include <functional>
class QParallelAnimationGroup;
namespace LuDash {
class WindowAnimations final : public QObject {
public:
    explicit WindowAnimations(QObject* parent = nullptr);
    void setDuration(int milliseconds);
    void show(QQuickItem* item);
    void hide(QQuickItem* item, const std::function<void()>& finished = {});
    void cancel(QQuickItem* item);
    int activeCount() const;
private:
    void animate(QQuickItem* item, bool showing, const std::function<void()>& finished);
    QHash<QQuickItem*, QPointer<QParallelAnimationGroup>> active_;
    int duration_ = 220;
};
}
