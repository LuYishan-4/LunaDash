#include <LuDash/application_catalog/ApplicationCatalog.h>
#include <LuDash/application_window/ApplicationWindow.h>
#include <LuDash/client_lifecycle/WaylandClientShutdown.h>
#include <LuDash/console/Console.h>
#include <LuDash/default_applications/DefaultApplications.h>
#include <LuDash/file_manager/FileManager.h>
#include <LuDash/launcher/Launcher.h>
#include <LuDash/localization/Localization.h>
#include <LuDash/packages/PackageManager.h>
#include <LuDash/plugin_settings/PluginSettings.h>
#include <LuDash/system_monitor/SystemMonitor.h>
#include <LuDash/theme/DesktopTheme.h>
#include <LuDash/welcome/Welcome.h>
#include <QProcess>
#include <QSurfaceFormat>
#include <QtWidgets>

int main(int argc, char **argv) {
  QSurfaceFormat format;
  format.setVersion(2, 1);
  format.setSwapInterval(1);
  QSurfaceFormat::setDefaultFormat(format);
  QApplication app(argc, argv);
  LuDash::WaylandClientShutdown shutdown(app);
  app.setApplicationName("LunaDash");
  app.setOrganizationName("LunaDash");
  app.setApplicationVersion("0.1.0");
  LuDash::initializeLocalization(app);
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
    if (requested == "terminal" ||
        (requested == "files" && !parser.isSet("builtin"))) {
      QString error;
      auto command = LuDash::defaultApplicationCommand(requested, &error);
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
    LuDash::ApplicationWindow window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    window.setAttribute(Qt::WA_StyledBackground);
    window.setObjectName("applicationWindow");
    LuDash::watchDesktopTheme(&window);
    QWidget *content = nullptr;
    const auto id = parser.value("app");
    if (id == "files")
      content = LuDash::createFileManager();
    else if (id == "console")
      content = LuDash::createConsole();
    else if (id == "monitor")
      content = LuDash::createSystemMonitor();
    else if (id == "packages")
      content = LuDash::createPackageManager();
    else if (id == "plugins")
      content = LuDash::createPluginSettings();
    else if (id == "launcher")
      content = LuDash::createLauncher();
    else if (id == "welcome")
      content = LuDash::createWelcome([](const QString &target) {
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
