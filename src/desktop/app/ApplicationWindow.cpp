#include "desktop/app/ApplicationWindow.hpp"
#include <QCloseEvent>
namespace LunaDash {
void ApplicationWindow::closeEvent(QCloseEvent *event) {
  if (canClose && !canClose())
    event->ignore();
  else
    event->accept();
}
} // namespace LunaDash
