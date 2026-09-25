#include "desktop/app/DesktopApplication.hpp"
#include "config/BuildConfig.hpp"
#include "config/localization/Localization.hpp"
#include "desktop/app/ApplicationWindow.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/app/WaylandClientShutdown.hpp"
#include "desktop/package/PackageManager.hpp"
#include "desktop/theme/DesktopTheme.hpp"
#include "desktop/welcome/Welcome.hpp"
#include <QProcess>
#include <QSurfaceFormat>
#include <QtWidgets>

namespace LunaDash {
int DesktopApplication::run(int argc, char **argv) {
  QSurfaceFormat format;
  format.setVersion(2, 1);
  format.setSwapInterval(1);
  QSurfaceFormat::setDefaultFormat(format);
  QApplication app(argc, argv);
  QApplication::setStyle("Fusion");
  LunaDash::WaylandClientShutdown shutdown(app);
  app.setApplicationName("LunaDash");
  app.setOrganizationName("LunaDash");
  app.setApplicationVersion(QStringLiteral(LUDASH_VERSION));
  LunaDash::initializeLocalization(app);
  QCommandLineParser parser;
  parser.setApplicationDescription("LunaDash — C++ / OpenGL Wayland desktop");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption(
      {"app", "Open a desktop tool or the configured file manager.", "id"});
  parser.addOption({"screenshot",
                    "Save a screenshot and exit (for UI verification).",
                    "path"});
  parser.addOption(
      {"path", "Folder to open in the configured file manager.", "folder"});
  parser.process(app);

  if (parser.isSet("app")) {
    const auto requested = parser.value("app");
    app.setDesktopFileName("lunadash-app");
    if (requested == "settings" || requested == "plugins")
      return QProcess::execute(QCoreApplication::applicationDirPath() +
                                   "/lunadashctl",
                               {"open-settings", requested == "plugins"
                                                     ? "plugins" : "general"});
    if (requested == "files") {
      QString error;
      auto command = LunaDash::defaultApplicationCommand(requested, &error);
      if (!error.isEmpty() || command.isEmpty()) {
        qCritical().noquote()
            << (error.isEmpty() ? "No file manager is configured." : error);
        return 2;
      }
      const auto program = command.takeFirst();
      if (parser.isSet("path"))
        command << QFileInfo(parser.value("path")).absoluteFilePath();
      return QProcess::execute(program, command);
    }
    LunaDash::ApplicationWindow window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    window.setAttribute(Qt::WA_StyledBackground);
    window.setObjectName("applicationWindow");
    window.setProperty("ludashFrosted", false);
    LunaDash::watchDesktopTheme(&window);
    QWidget *content = nullptr;
    const auto id = parser.value("app");
    if (id == "packages")
      content = LunaDash::createPackageManager();
    else if (id == "welcome")
      content = LunaDash::createWelcome([](const QString &target) {
        QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                {"--app", target});
      });
    if (!content) {
      qCritical("Unknown built-in application.");
      return 2;
    }
    content->setObjectName("applicationContent");
    content->setAttribute(Qt::WA_StyledBackground);
    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(content);
    window.setWindowTitle("LunaDash · " + LunaDash::translate(
        id == "welcome" ? "Welcome" : "Packages"));
    window.resize(760, 520);
    window.show();
    if (parser.isSet("screenshot"))
      QTimer::singleShot(1200, &window, [&] {
        app.exit(window.grab().save(parser.value("screenshot")) ? 0 : 2);
      });
    return app.exec();
  }
  auto compositor =
      QCoreApplication::applicationDirPath() + "/lunadash-compositor";
  if (!QFileInfo::exists(compositor))
    compositor = QCoreApplication::applicationDirPath() + "/ludash-compositor";
  return QProcess::execute(compositor, {});
}

} // namespace LunaDash
