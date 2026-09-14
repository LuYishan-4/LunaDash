#pragma once
#include <QQuickPaintedItem>
#include <functional>
namespace LuDash {
enum class WindowFrameAction { Focus, Minimize, Close };

class WindowFrame final : public QQuickPaintedItem {
public:
  explicit WindowFrame(QQuickItem *parent);
  QString title;
  QColor accent = QColor("#7dcccf");
  bool focused = false;
  std::function<void(WindowFrameAction)> action;
  void paint(QPainter *painter) override;

protected:
  void mousePressEvent(QMouseEvent *event) override;
};
} // namespace LuDash
