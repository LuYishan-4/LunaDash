#include <LuDash/window_frame/WindowFrame.h>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

namespace LuDash {
WindowFrame::WindowFrame(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAcceptedMouseButtons(Qt::LeftButton);
}
void WindowFrame::paint(QPainter *painter) {
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(
      QPen(focused ? accent : QColor("#45666a"), focused ? 1.5 : 1.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawRoundedRect(boundingRect().adjusted(.75, .75, -.75, -.75), 14,
                           14);
  painter->fillRect(QRectF(1, 1, width() - 2, 23), QColor(20, 28, 38, 220));
  painter->setPen(QColor(focused ? "#b9dfdc" : "#8eaaa9"));
  QFont font("sans-serif");
  font.setPixelSize(11);
  painter->setFont(font);
  painter->drawText(QRectF(12, 0, std::max(0.0, width() - 72), 24),
                    Qt::AlignVCenter, title);
  painter->drawText(QRectF(width() - 56, 0, 28, 24), Qt::AlignCenter, "−");
  painter->drawText(QRectF(width() - 28, 0, 28, 24), Qt::AlignCenter, "×");
}
void WindowFrame::mousePressEvent(QMouseEvent *event) {
  if (action) {
    const qreal x = event->position().x();
    if (x >= width() - 28)
      action(WindowFrameAction::Close);
    else if (x >= width() - 56)
      action(WindowFrameAction::Minimize);
    else
      action(WindowFrameAction::Focus);
  }
  event->accept();
}
} // namespace LuDash
