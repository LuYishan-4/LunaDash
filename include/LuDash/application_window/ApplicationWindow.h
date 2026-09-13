#pragma once
#include <QWidget>
#include <functional>
namespace LuDash {
class ApplicationWindow final : public QWidget {
public:
    std::function<bool()> canClose;
protected:
    void closeEvent(QCloseEvent* event) override;
};
}
