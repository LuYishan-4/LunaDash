#pragma once

#include <QColor>
#include <QQuickPaintedItem>

class QPainter;

namespace LuDash {

class ResizeGuideItem final : public QQuickPaintedItem {
public:
  explicit ResizeGuideItem(QQuickItem *parent);

  void setGuide(const QRect &geometry, const QColor &color);
  void paint(QPainter *painter) override;

private:
  QColor accent_ = QColor("#9ccbfb");
};

} // namespace LuDash
