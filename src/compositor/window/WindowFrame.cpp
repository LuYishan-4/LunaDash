#include "compositor/window/WindowFrame.hpp"
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <algorithm>

namespace LunaDash {
WindowFrame::WindowFrame(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAcceptedMouseButtons(Qt::LeftButton);
  moveX_ = new QPropertyAnimation(this, "x", this);
  moveY_ = new QPropertyAnimation(this, "y", this);
  for (auto *animation : {moveX_, moveY_}) {
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setDuration(0);
  }
}
void WindowFrame::moveTo(const QPoint &position, bool animated) {
  const int duration = animated ? 220 : 0;
  moveX_->stop();
  moveY_->stop();
  moveX_->setDuration(duration);
  moveY_->setDuration(duration);
  moveX_->setStartValue(x());
  moveX_->setEndValue(qreal(position.x()));
  moveY_->setStartValue(y());
  moveY_->setEndValue(qreal(position.y()));
  if (duration == 0) {
    setX(position.x());
    setY(position.y());
    return;
  }
  moveX_->start();
  moveY_->start();
}
void WindowFrame::paint(QPainter *painter) {
  Q_UNUSED(painter);
  // Borderless windows: the frame is transparent and the client surface fills
  // it, so no title bar, border or window controls are drawn. Closing and
  // minimizing are left to the keyboard shortcuts and each application's own
  // UI instead.
}
void WindowFrame::mousePressEvent(QMouseEvent *event) {
  if (action)
    action(WindowFrameAction::Focus);
  event->accept();
}
} // namespace LunaDash
