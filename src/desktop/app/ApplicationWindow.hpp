#pragma once
#include <QWidget>
#include <functional>
namespace LunaDash {
class ApplicationWindow final : public QWidget {
public:
  std::function<bool()> canClose;

protected:
  void closeEvent(QCloseEvent *event) override;
};
} // namespace LunaDash
