#include "desktop/ApplicationWindow/ApplicationWindow.hpp"
#include <QCloseEvent>
namespace LuDash {
void ApplicationWindow::closeEvent(QCloseEvent* event) {
    if (canClose && !canClose()) event->ignore();
    else event->accept();
}
}
