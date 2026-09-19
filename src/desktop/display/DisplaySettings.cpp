#include "desktop/display/DisplaySettings.hpp"
#include <QGuiApplication>
#include <QMap>
#include <QScreen>
#include <QWindow>
namespace LunaDash {
QJsonObject describeDisplay(const QWindow *window) {
  const auto *screen = window->screen();
  return {{"width", window->width()},
          {"height", window->height()},
          {"scale", window->devicePixelRatio()},
          {"refreshRate", screen ? screen->refreshRate() : 0},
          {"output", screen ? screen->name() : QString()},
          {"nested", QGuiApplication::platformName() != "eglfs"},
          {"fullscreen", window->visibility() == QWindow::FullScreen}};
}
bool resizeNestedDesktop(QWindow *window, const QString &preset,
                         QString *error) {
  const QMap<QString, QSize> sizes{{"1280x720", {1280, 720}},
                                   {"1440x900", {1440, 900}},
                                   {"1920x1080", {1920, 1080}}};
  if (!sizes.contains(preset) || QGuiApplication::platformName() == "eglfs" ||
      window->visibility() == QWindow::FullScreen) {
    if (error)
      *error = "Choose a supported size in a windowed nested session.";
    return false;
  }
  window->resize(sizes.value(preset));
  return true;
}
} // namespace LunaDash
