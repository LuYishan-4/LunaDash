#pragma once

#include <QColor>
#include <QQuickPaintedItem>

class QPainter;

namespace LunaDash {

class ResizeGuide final : public QQuickPaintedItem {
public:
  explicit ResizeGuide(QQuickItem *parent);

  void setGuide(const QRect &geometry, const QColor &color);
  void paint(QPainter *painter) override;

private:
  QColor accent_ = QColor("#9ccbfb");
};

} // namespace LunaDash
