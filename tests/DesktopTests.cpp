#include <LuDash/plugins/PluginManager.h>
#include <LuDash/wallpaper/WallpaperSettings.h>
#include <LuDash/packages/PackageManager.h>
#include <LuDash/localization/Localization.h>
#include <LuDash/application_window/ApplicationWindow.h>
#include <LuDash/application_catalog/ApplicationCatalog.h>
#include <LuDash/file_manager/FileManager.h>
#include <LuDash/console/Console.h>
#include <LuDash/system_monitor/SystemMonitor.h>
#include <LuDash/welcome/Welcome.h>
#include <LuDash/tiling/TilingLayout.h>
#include <QtTest>
#include <QtWidgets>

namespace LuDash {
class DesktopTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        qputenv("LUDASH_LANGUAGE", "en_US");
        LuDash::initializeLocalization(*QCoreApplication::instance());
    }
    void languagePackIsExternal() {
        const auto dictionary = LuDash::languageDictionary("zh_TW");
        QVERIFY(dictionary.contains("Files"));
        QVERIFY(dictionary.value("Files").toString() != "Files");
        QCOMPARE(LuDash::translate("Files"), QString("Files"));
    }
    void wallpaperRejectsInvalidFiles() {
        QTemporaryDir directory;
        QString error;
        QVERIFY(!LuDash::setWallpaperImage(directory.path(), &error));
        QVERIFY(!error.isEmpty());
        QFile file(directory.filePath("invalid.png")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not an image"); file.close();
        QVERIFY(!LuDash::setWallpaperImage(file.fileName(), &error));
        QVERIFY(!LuDash::setWallpaperImage(directory.filePath("missing.png"), &error));
    }
    void packageArgumentsRejectOptionInjection() {
        QCOMPARE(LuDash::packageTransactionArguments("install", "fcitx5"), QStringList({"-Syu", "--", "fcitx5"}));
        QVERIFY(LuDash::packageTransactionArguments("install", "--root=/tmp").isEmpty());
        QVERIFY(LuDash::packageTransactionArguments("remove", "foo;id").isEmpty());
        QVERIFY(LuDash::packageTransactionArguments("unknown", "foo").isEmpty());
    }
    void pluginMetadataRejectsTraversal() {
        QTemporaryDir directory;
        QFile metadata(directory.filePath("metadata.json")); QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write(R"({"KPlugin":{"Id":"org.test.effect","Name":"Test","Version":"1"},"LuDash":{"ApiVersion":1,"Type":"WindowEffect","Library":"../outside.so"}})"); metadata.close();
        const auto plugin = LuDash::readPluginMetadata(metadata.fileName());
        QVERIFY(!plugin.error.isEmpty()); QVERIFY(!plugin.enabled);
    }
    void tilingDoesNotOverlap() {
        const QRect area(16, 62, 1408, 742);
        for (int count = 1; count <= 12; ++count) {
            const auto tiles = LuDash::tileRectangles(area, count);
            QCOMPARE(tiles.size(), count);
            for (int i = 0; i < tiles.size(); ++i) {
                QVERIFY(area.contains(tiles[i])); QVERIFY(tiles[i].width() > 0); QVERIFY(tiles[i].height() > 0);
                for (int j = i + 1; j < tiles.size(); ++j) QVERIFY(!tiles[i].intersects(tiles[j]));
            }
        }
        QVERIFY(LuDash::tileRectangles(area, 0).isEmpty());
    }
    void applicationWindowLifecycle() {
        auto* window = new LuDash::ApplicationWindow;
        window->setAttribute(Qt::WA_DeleteOnClose); window->show();
        window->hide(); QVERIFY(!window->isVisible()); window->show(); QVERIFY(window->isVisible());
        QPointer<QWidget> reference = window; window->close(); QTRY_VERIFY(reference.isNull());
    }
    void fileNavigation() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        std::unique_ptr<QWidget> files(LuDash::createFileManager()); files->show();
        auto* input = files->findChild<QLineEdit*>("fileLocation");
        input->setText(directory.path()); QTest::keyClick(input, Qt::Key_Return);
        auto* tree = files->findChild<QTreeView*>("fileView");
        auto* model = qobject_cast<QFileSystemModel*>(tree->model());
        QCOMPARE(model->filePath(tree->rootIndex()), directory.path());
    }
    void retiredNotesAreNotListed() {
        const auto apps = builtinApplications();
        QVERIFY(std::none_of(apps.begin(), apps.end(), [](const auto& app) { return app.id == "notes"; }));
    }
    void commandRunsAndReportsExit() {
        std::unique_ptr<QWidget> console(LuDash::createConsole()); console->show();
        auto* input = console->findChild<QLineEdit*>("consoleInput");
        auto* output = console->findChild<QPlainTextEdit*>("consoleOutput");
        input->setText("printf ludash-test-marker; exit 7"); QTest::keyClick(input, Qt::Key_Return);
        QTRY_VERIFY(output->toPlainText().contains("[Exit code 7]"));
        QVERIFY(output->toPlainText().contains("ludash-test-marker")); QVERIFY(input->isEnabled());
    }
};
}
QTEST_MAIN(LuDash::DesktopTests)
#include "DesktopTests.moc"
