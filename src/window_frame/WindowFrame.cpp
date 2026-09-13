#include <LuDash/window_frame/WindowFrame.h>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

namespace LuDash {
WindowFrame::WindowFrame(QQuickItem* parent) : QQuickPaintedItem(parent) { setAcceptedMouseButtons(Qt::LeftButton); }
void WindowFrame::paint(QPainter* painter) {
    painter->fillRect(boundingRect(), QColor(focused ? "#829aca" : "#3c4962"));
    painter->fillRect(QRectF(1, 1, width() - 2, 29), QColor("#252d40"));
    painter->setPen(QColor("#e4eaf8"));
    painter->drawText(QRectF(14, 0, width() - 54, 30), Qt::AlignVCenter, title);
    painter->drawText(QRectF(width() - 32, 0, 30, 30), Qt::AlignCenter, "×");
}
void WindowFrame::mousePressEvent(QMouseEvent* event) { if (clicked) clicked(event->position().x() > width() - 32); event->accept(); }
}
