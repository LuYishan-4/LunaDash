#include <LuDash/compositor/WaylandCompositor.h>
#include <LuDash/localization/Localization.h>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>

int main(int argc, char **argv) {
  const auto graphics = LuDash::graphicsApiFromArguments(argc, argv);
  if (!graphics) {
    qCritical("--graphics must be auto, opengl, or gles");
    return 2;
  }
  LuDash::configureGraphics(*graphics);
  QGuiApplication app(argc, argv);
  app.setApplicationName("LunaDah");
  app.setOrganizationName("LunaDah");
  app.setDesktopFileName("lunadah-app");
  LuDash::initializeLocalization(app);
  QCommandLineParser parser;
  parser.setApplicationDescription(
      "LunaDah native Wayland tiling compositor (OpenGL)");
  parser.addHelpOption();
  parser.addOption({"graphics",
                    "Graphics context: auto, opengl (3.3+), or gles (3.0+).",
                    "api", "auto"});
  parser.addOption({"socket", "Wayland socket name.", "name", "lunadah-0"});
  parser.addOption({"fullscreen", "Use the entire host output."});
  parser.addOption({"no-shell", "Do not start the desktop shell."});
  parser.addOption({"demo", "Start two demonstration clients."});
  parser.addOption(
      {"screenshot", "Save compositor screenshot at exit.", "path"});
  parser.addOption({"state", "Write window geometry JSON at exit.", "path"});
  parser.addOption(
      {"exit-after", "Exit after this many milliseconds (test mode).", "ms"});
  parser.process(app);
  LuDash::WaylandCompositor compositor(parser.value("socket").toUtf8(),
                                       parser.isSet("fullscreen"),
                                       !parser.isSet("no-shell"), *graphics);
  if (parser.isSet("demo"))
    QTimer::singleShot(900, &app, [&] {
      if (!parser.isSet("no-shell"))
        compositor.spawn({"--app", "welcome"});
      compositor.spawn({"--app", "files"});
      compositor.spawn({"--app", "monitor"});
    });
  if (parser.isSet("exit-after"))
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
  return app.exec();
}
