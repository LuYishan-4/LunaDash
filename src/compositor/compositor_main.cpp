#include "compositor/WaylandCompositor/WaylandCompositor.hpp"
#include "desktop/InputSettings/InputSettings.hpp"
#include "config/Localization/Localization.hpp"
#include <QAbstractEventDispatcher>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>

int main(int argc, char **argv) {
  const auto graphics = LuDash::graphicsApiFromArguments(argc, argv);
  if (!graphics) { qCritical("--graphics must be auto, opengl, or gles"); return 2; }
  LuDash::configureGraphics(*graphics);
  QGuiApplication app(argc, argv);
  app.setApplicationName("LunaDash"); app.setOrganizationName("LunaDash"); app.setDesktopFileName("lunadash-app");
  LuDash::initializeLocalization(app);
  QCommandLineParser parser;
  parser.setApplicationDescription("LunaDash native Wayland tiling compositor (OpenGL)");
  parser.addHelpOption();
  parser.addOption({"graphics", "Graphics context: auto, opengl (3.3+), or gles (3.0+).", "api", "auto"});
  parser.addOption({"socket", "Wayland socket name.", "name", "lunadash-0"});
  parser.addOption({"nested", "Run inside an existing compositor and use Qt host input."});
  parser.addOption({"profile", "Log compositor event-loop stalls longer than 25 ms."});
  parser.addOption({"fullscreen", "Use the entire host output."}); parser.addOption({"no-shell", "Do not start the desktop shell."}); parser.addOption({"demo", "Start two demonstration clients."});
  parser.addOption({"screenshot", "Save compositor screenshot at exit.", "path"}); parser.addOption({"state", "Write window geometry JSON at exit.", "path"}); parser.addOption({"exit-after", "Exit after this many milliseconds (test mode).", "ms"});
  parser.process(app);

  // Only own libinput directly on bare-metal QPA backends. When LunaDash runs
  // on Wayland/X11, Qt already receives keyboard events from the host
  // compositor. Opening seat0 with libinput at the same time forwards every
  // physical key through two independent paths, which causes missed-looking
  // taps, duplicated characters and broken repeat timing.
  const QString platform = QGuiApplication::platformName().toLower();
  const bool bareMetalInput =
      platform == QStringLiteral("eglfs") ||
      platform == QStringLiteral("linuxfb") ||
      platform == QStringLiteral("kms") ||
      platform == QStringLiteral("vkkhrdisplay");
  LuDash::setNativeKeyboardInputEnabled(!parser.isSet("nested") && bareMetalInput);
  qInfo().noquote() << "LunaDash keyboard input:"
                    << ((!parser.isSet("nested") && bareMetalInput)
                            ? "native libinput"
                            : "Qt host input")
                    << "(QPA" << platform + ")";

  QElapsedTimer activeTurn;
  if (parser.isSet("profile")) {
    auto *dispatcher = QAbstractEventDispatcher::instance();
    QObject::connect(dispatcher, &QAbstractEventDispatcher::awake, &app, [&] { activeTurn.restart(); });
    QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, &app, [&] {
      if (activeTurn.isValid()) { const qint64 elapsed = activeTurn.elapsed(); if (elapsed > 25) qWarning().noquote() << "LunaDash event-loop stall:" << elapsed << "ms"; }
    });
  }
  LuDash::WaylandCompositor compositor(parser.value("socket").toUtf8(), parser.isSet("fullscreen"), !parser.isSet("no-shell"), *graphics);
  if (parser.isSet("demo")) QTimer::singleShot(900, &app, [&] { if (!parser.isSet("no-shell")) compositor.spawn({"--app", "welcome"}); compositor.spawn({"--app", "files"}); compositor.spawn({"--app", "monitor"}); });
  if (parser.isSet("exit-after")) QTimer::singleShot(std::max(1000, parser.value("exit-after").toInt()), &app, [&] { if (parser.isSet("state")) compositor.saveState(parser.value("state")); bool ok = true; if (parser.isSet("screenshot")) ok = compositor.saveScreenshot(parser.value("screenshot")); compositor.closeTestSession([&, ok](bool clean) { app.exit(ok && clean && !compositor.hasProcessFailure() ? 0 : 2); }); });
  return app.exec();
}
