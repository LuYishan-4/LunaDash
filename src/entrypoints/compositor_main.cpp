#include <LuDash/localization/Localization.h>
#include <LuDash/compositor/WaylandCompositor.h>
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>

int main(int argc, char** argv) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv); app.setApplicationName("LuDash"); app.setOrganizationName("LuDash");
    LuDash::initializeLocalization(app);
    QCommandLineParser parser; parser.setApplicationDescription("LuDash native Wayland tiling compositor (OpenGL)"); parser.addHelpOption();
    parser.addOption({"socket", "Wayland socket name.", "name", "ludash-0"});
    parser.addOption({"fullscreen", "Use the entire host output."});
    parser.addOption({"no-shell", "Do not start the desktop shell."});
    parser.addOption({"demo", "Start two demonstration clients."});
    parser.addOption({"screenshot", "Save compositor screenshot at exit.", "path"});
    parser.addOption({"state", "Write window geometry JSON at exit.", "path"});
    parser.addOption({"exit-after", "Exit after this many milliseconds (test mode).", "ms"}); parser.process(app);
    LuDash::WaylandCompositor compositor(parser.value("socket").toUtf8(), parser.isSet("fullscreen"), !parser.isSet("no-shell"));
    if (parser.isSet("demo")) QTimer::singleShot(900, &app, [&] { compositor.spawn({"--app", "files"}); compositor.spawn({"--app", "monitor"}); });
    if (parser.isSet("exit-after")) QTimer::singleShot(std::max(1000, parser.value("exit-after").toInt()), &app, [&] {
        if (parser.isSet("state")) compositor.saveState(parser.value("state"));
        bool ok = true; if (parser.isSet("screenshot")) ok = compositor.saveScreenshot(parser.value("screenshot"));
        compositor.closeTestSession([&, ok](bool clean) { app.exit(ok && clean && !compositor.hasProcessFailure() ? 0 : 2); });
    });
    return app.exec();
}
