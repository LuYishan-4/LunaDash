#include <QApplication>
#include <QLabel>
#include <QMainWindow>

namespace LunaDash {
int runQtToolkitClient(int argc, char **argv) {
  QApplication application(argc, argv);
  QMainWindow window;
  window.setWindowTitle("LunaDash Qt 6");
  window.setCentralWidget(new QLabel("Wayland toolkit lifecycle probe"));
  window.resize(420, 280);
  window.show();
  return application.exec();
}
} // namespace LunaDash

int main(int argc, char **argv) {
  return LunaDash::runQtToolkitClient(argc, argv);
}
