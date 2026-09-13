#include <LuDash/window_frame/WindowFrame.h>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

namespace LuDash {
WindowFrame::WindowFrame(QQuickItem* parent) : QQuickPaintedItem(parent) { setAcceptedMouseButtons(Qt::LeftButton); }
void WindowFrame::paint(QPainter* painter) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(QColor(focused ? "#7dcccf" : "#45666a"), focused ? 1.5 : 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(boundingRect().adjusted(.75, .75, -.75, -.75), 5, 5);
    painter->fillRect(QRectF(1, 1, width() - 2, 23), QColor(19, 31, 32, 220));
    painter->setPen(QColor(focused ? "#b9dfdc" : "#8eaaa9"));
    QFont font("monospace"); font.setPixelSize(11); painter->setFont(font);
    painter->drawText(QRectF(12, 0, width() - 50, 24), Qt::AlignVCenter, title);
    painter->drawText(QRectF(width() - 28, 0, 26, 24), Qt::AlignCenter, "×");
}
void WindowFrame::mousePressEvent(QMouseEvent* event) { if (clicked) clicked(event->position().x() > width() - 32); event->accept(); }
}
