#include "desktop/app/DesktopApplication.hpp"
#include "config/BuildConfig.hpp"
#include "config/localization/Localization.hpp"
#include "desktop/app/ApplicationCatalog.hpp"
#include "desktop/app/ApplicationWindow.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/app/WaylandClientShutdown.hpp"
#include "desktop/console/Console.hpp"
#include "desktop/filemanager/FileManager.hpp"
#include "desktop/launcher/Launcher.hpp"
#include "desktop/package/PackageManager.hpp"
#include "desktop/plugins/PluginSettings.hpp"
#include "desktop/system/SystemMonitor.hpp"
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
      {"app", "Open a built-in application as a native Wayland window.", "id"});
  parser.addOption({"screenshot",
                    "Save a screenshot and exit (for UI verification).",
                    "path"});
  parser.addOption(
      {"builtin",
       "Open the built-in file manager, ignoring the default application."});
  parser.addOption({"path", "Folder to open in Files.", "folder"});
  parser.process(app);

  if (parser.isSet("app")) {
    const auto requested = parser.value("app");
    app.setDesktopFileName("lunadash-app");
    if (requested == "settings")
      return QProcess::execute(QCoreApplication::applicationDirPath() +
                                   "/lunadashctl",
                               {"open-settings"});
    if (requested == "files" && !parser.isSet("builtin")) {
      QString error;
      auto command = LunaDash::defaultApplicationCommand(requested, &error);
      if (!error.isEmpty()) {
        qCritical().noquote() << error;
        return 2;
      }
      if (!command.isEmpty()) {
        const auto program = command.takeFirst();
        if (requested == "files" && parser.isSet("path"))
          command << QFileInfo(parser.value("path")).absoluteFilePath();
        return QProcess::execute(program, command);
      }
    }
    LunaDash::ApplicationWindow window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    window.setAttribute(Qt::WA_StyledBackground);
    window.setObjectName("applicationWindow");
    window.setProperty("ludashFrosted", false);
    LunaDash::watchDesktopTheme(&window);
    QWidget *content = nullptr;
    const auto id = parser.value("app");
    if (id == "files")
      content = LunaDash::createFileManager();
    else if (id == "console")
      content = LunaDash::createConsole();
    else if (id == "monitor")
      content = LunaDash::createSystemMonitor();
    else if (id == "packages")
      content = LunaDash::createPackageManager();
    else if (id == "plugins")
      content = LunaDash::createPluginSettings();
    else if (id == "launcher")
      content = LunaDash::createLauncher();
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
    if (id == "files" && parser.isSet("path")) {
      auto *location = content->findChild<QLineEdit *>("fileLocation");
      location->setText(QFileInfo(parser.value("path")).absoluteFilePath());
      QMetaObject::invokeMethod(location, "returnPressed");
    }
    window.setWindowTitle("LunaDash · " + id);
    window.resize(id == "files" ? 1040 : 760, id == "files" ? 680 : 520);
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
