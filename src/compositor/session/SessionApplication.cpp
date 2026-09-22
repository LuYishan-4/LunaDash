#include "compositor/session/SessionApplication.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "config/localization/Localization.hpp"
#include "config/BuildConfig.hpp"

#include <QAbstractEventDispatcher>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <algorithm>
#include <csignal>
#include <unistd.h>

namespace {
void fatalSignalHandler(int signalNumber) {
  static constexpr char message[] =
      "LunaDash compositor received a fatal signal.\n";
  ::write(STDERR_FILENO, message, sizeof(message) - 1);
  ::signal(signalNumber, SIG_DFL);
  ::kill(::getpid(), signalNumber);
}

void installFatalSignalDiagnostics() {
  for (const int signalNumber :
       {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE}) {
    struct sigaction action {};
    action.sa_handler = fatalSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;
    sigaction(signalNumber, &action, nullptr);
  }
}
} // namespace

namespace LunaDash {
int SessionApplication::run(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  installFatalSignalDiagnostics();
  QObject::connect(&app, &QCoreApplication::aboutToQuit, [] {
    qInfo("LunaDash compositor event loop is quitting cleanly.");
  });
  app.setApplicationName("LunaDash");
  app.setOrganizationName("LunaDash");
  app.setApplicationVersion(QString::fromLatin1(BuildConfig::version) + " (" +
                            QString::fromLatin1(BuildConfig::commit) + ")");
  LunaDash::initializeLocalization(app);

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "LunaDash wlroots Wayland compositor");
  parser.addHelpOption();
  parser.addVersionOption();
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

  qInfo().noquote() << "LunaDash version:" << app.applicationVersion()
                    << "executable:" << app.applicationFilePath();
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
    });
  }

  if (parser.isSet("exit-after")) {
    QTimer::singleShot(
        std::max(1000, parser.value("exit-after").toInt()), &app, [&] {
          if (parser.isSet("state"))
            compositor.saveState(parser.value("state"));
          const auto finish = [&app, &compositor](bool ok) {
            compositor.closeTestSession([&app, &compositor, ok](bool clean) {
              app.exit(ok && clean && !compositor.hasProcessFailure() ? 0 : 2);
            });
          };
          if (!parser.isSet("screenshot")) {
            finish(true);
            return;
          }
          if (!compositor.saveScreenshot(parser.value("screenshot"), finish))
            finish(false);
        });
  }

  return app.exec();
}

} // namespace LunaDash
