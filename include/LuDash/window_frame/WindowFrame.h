#pragma once
#include <QQuickPaintedItem>
#include <functional>
namespace LuDash {
class WindowFrame final : public QQuickPaintedItem {
public:
    explicit WindowFrame(QQuickItem* parent);
    QString title;
    bool focused = false;
    std::function<void(bool)> clicked;
    void paint(QPainter* painter) override;
protected:
    void mousePressEvent(QMouseEvent* event) override;
};
}
