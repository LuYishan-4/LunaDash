#include "compositor/window/ResizeGuide.hpp"

#include <QPainter>
#include <QPen>

namespace LunaDash {

ResizeGuide::ResizeGuide(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAcceptedMouseButtons(Qt::NoButton);
  setVisible(false);
  setZ(900);
}

void ResizeGuide::setGuide(const QRect &geometry, const QColor &color) {
  accent_ = color;
  setPosition(geometry.topLeft());
  setSize(geometry.size());
  update();
}

void ResizeGuide::paint(QPainter *painter) {
  painter->setRenderHint(QPainter::Antialiasing, true);

  QColor fill = accent_;
  fill.setAlphaF(0.08);

  painter->setBrush(fill);

  QPen pen(accent_);
  pen.setWidthF(2.0);
  pen.setStyle(Qt::DashLine);

  painter->setPen(pen);
  painter->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 14, 14);
}

} // namespace LunaDash
