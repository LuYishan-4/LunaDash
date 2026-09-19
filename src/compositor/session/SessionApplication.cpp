#include "compositor/session/SessionApplication.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "config/localization/Localization.hpp"

#include <QAbstractEventDispatcher>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <algorithm>

namespace LunaDash {
int SessionApplication::run(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  app.setApplicationName("LunaDash");
  app.setOrganizationName("LunaDash");
  LunaDash::initializeLocalization(app);

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "LunaDash wlroots Wayland tiling compositor");
  parser.addHelpOption();
  parser.addOption({"graphics",
                    "Renderer preference: auto, opengl or gles. OpenGL and "
                    "GLES select wlroots' GLES2 renderer.",
                    "api", "auto"});
  parser.addOption({"socket", "Wayland socket name.", "name", "lunadash-0"});
  parser.addOption(
      {"nested", "Run with the wlroots nested backend selected by the host."});
  parser.addOption(
      {"profile", "Log compositor event-loop stalls longer than 25 ms."});
  parser.addOption(
      {"fullscreen", "Prefer the host output size in nested mode."});
  parser.addOption({"no-shell", "Do not start the desktop shell."});
  parser.addOption({"demo", "Start two demonstration clients."});
  parser.addOption(
      {"screenshot", "Save compositor screenshot at exit.", "path"});
  parser.addOption({"state", "Write window geometry JSON at exit.", "path"});
  parser.addOption({"exit-after", "Exit after this many milliseconds.", "ms"});
  parser.process(app);

  const QString renderer = parser.value("graphics").toLower();
  if (renderer != "auto" && renderer != "opengl" && renderer != "gles") {
    qCritical("--graphics must be auto, opengl, or gles");
    return 2;
  }

  qInfo("LunaDash input: wlroots seat + libinput/xkbcommon (no Qt input path)");

  QElapsedTimer activeTurn;
  if (parser.isSet("profile")) {
    auto *dispatcher = QAbstractEventDispatcher::instance();
    QObject::connect(dispatcher, &QAbstractEventDispatcher::awake, &app,
                     [&] { activeTurn.restart(); });
    QObject::connect(
        dispatcher, &QAbstractEventDispatcher::aboutToBlock, &app, [&] {
          if (!activeTurn.isValid())
            return;
          const qint64 elapsed = activeTurn.elapsed();
          if (elapsed > 25)
            qWarning() << "LunaDash event-loop stall:" << elapsed << "ms";
        });
  }

  LunaDash::WaylandCompositor compositor(parser.value("socket").toUtf8(),
                                         parser.isSet("fullscreen"),
                                         !parser.isSet("no-shell"), renderer);

  if (parser.isSet("demo")) {
    QTimer::singleShot(900, &app, [&] {
      if (!parser.isSet("no-shell"))
        compositor.spawn({"--app", "welcome"});
      compositor.spawn({"--app", "files"});
      compositor.spawn({"--app", "monitor"});
    });
  }

  if (parser.isSet("exit-after")) {
    QTimer::singleShot(
        std::max(1000, parser.value("exit-after").toInt()), &app, [&] {
          if (parser.isSet("state"))
            compositor.saveState(parser.value("state"));
          bool ok = true;
          if (parser.isSet("screenshot"))
            ok = compositor.saveScreenshot(parser.value("screenshot"));
          compositor.closeTestSession([&, ok](bool clean) {
            app.exit(ok && clean && !compositor.hasProcessFailure() ? 0 : 2);
          });
        });
  }

  return app.exec();
}

} // namespace LunaDash
