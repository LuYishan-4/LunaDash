#pragma once
#include <QPoint>
#include <QQuickPaintedItem>
#include <functional>
class QPropertyAnimation;
namespace LuDash {
enum class WindowFrameAction { Focus, Minimize, Close };

class WindowFrame final : public QQuickPaintedItem {
public:
  explicit WindowFrame(QQuickItem *parent);
  QString title;
  QColor accent = QColor("#7dcccf");
  bool focused = false;
  std::function<void(WindowFrameAction)> action;
  void moveTo(const QPoint &position, bool animated);
  void paint(QPainter *painter) override;

protected:
  void mousePressEvent(QMouseEvent *event) override;

private:
  QPropertyAnimation *moveX_ = nullptr;
  QPropertyAnimation *moveY_ = nullptr;
};
} // namespace LuDash
