#include <LuDash/packages/PackageManager.h>
#include <LuDash/plugin_settings/PluginSettings.h>
#include <LuDash/localization/Localization.h>
#include <LuDash/launcher/Launcher.h>
#include <LuDash/application_window/ApplicationWindow.h>
#include <LuDash/theme/DesktopTheme.h>
#include <LuDash/application_catalog/ApplicationCatalog.h>
#include <LuDash/file_manager/FileManager.h>
#include <LuDash/console/Console.h>
#include <LuDash/system_monitor/SystemMonitor.h>
#include <LuDash/welcome/Welcome.h>
#include <QtWidgets>
#include <QSurfaceFormat>
#include <QProcess>

int main(int argc, char** argv) {
    QSurfaceFormat format; format.setVersion(2, 1); format.setSwapInterval(1); QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setApplicationName("LuDash"); app.setOrganizationName("LuDash"); app.setApplicationVersion("0.1.0");
    LuDash::initializeLocalization(app);
    QCommandLineParser parser; parser.setApplicationDescription("LuDash — C++ / OpenGL Wayland desktop"); parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"app", "Open a built-in application as a native Wayland window.", "id"});
    parser.addOption({"screenshot", "Save a screenshot and exit (for UI verification).", "path"});
    parser.process(app);

    if (parser.isSet("app")) {
        app.setDesktopFileName("ludash-app");
        if (parser.value("app") == "settings") return QProcess::execute(QCoreApplication::applicationDirPath() + "/ludashctl", {"open-settings"});
        LuDash::ApplicationWindow window;
        window.setAttribute(Qt::WA_TranslucentBackground);
        window.setAttribute(Qt::WA_StyledBackground);
        window.setObjectName("applicationWindow");
        window.setStyleSheet(LuDash::desktopStyle() + "QWidget#applicationWindow { background: rgba(18, 26, 36, 215); border-radius: 14px; }");
        QWidget* content = nullptr; const auto id = parser.value("app");
        if (id == "files") content = LuDash::createFileManager();
        else if (id == "console") content = LuDash::createConsole();
        else if (id == "monitor") content = LuDash::createSystemMonitor();
        else if (id == "packages") content = LuDash::createPackageManager();
        else if (id == "plugins") content = LuDash::createPluginSettings();
        else if (id == "launcher") content = LuDash::createLauncher();
        else if (id == "welcome") content = LuDash::createWelcome([](const QString& target) { QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--app", target}); });
        if (!content) { qCritical("Unknown built-in application."); return 2; }
        auto* layout = new QVBoxLayout(&window); layout->setContentsMargins(0, 0, 0, 0); layout->addWidget(content);
        window.setWindowTitle("LuDash · " + id); window.resize(760, 520); window.show();
        if (parser.isSet("screenshot")) QTimer::singleShot(1200, &window, [&] { app.exit(window.grab().save(parser.value("screenshot")) ? 0 : 2); });
        return app.exec();
    }
    return QProcess::execute(QCoreApplication::applicationDirPath() + "/ludash-compositor", {});
}
