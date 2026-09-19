#pragma once
#include <QHash>
#include <QObject>
#include <QQuickItem>
#include <functional>
class QParallelAnimationGroup;
namespace LunaDash {
class WindowAnimations final : public QObject {
public:
  explicit WindowAnimations(QObject *parent = nullptr);
  ~WindowAnimations() override;
  void setDuration(int milliseconds);
  void show(QQuickItem *item);
  void hide(QQuickItem *item, const std::function<void()> &finished = {});
  void cancel(QQuickItem *item);
  int activeCount() const;

private:
  void animate(QQuickItem *item, bool showing,
               const std::function<void()> &finished);
  // Items own the groups; each group's destroyed signal removes this lookup.
  QHash<QQuickItem *, QParallelAnimationGroup *> active_;
  int duration_ = 220;
};
} // namespace LunaDash
