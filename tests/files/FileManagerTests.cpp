#include "desktop/FileAssociations/FileAssociations.hpp"
#include "desktop/FileAssociationUi/FileAssociationUi.hpp"
#include "desktop/FileManager/FileManager.hpp"
#include "desktop/FileOperations/FileOperations.hpp"
#include <QtTest>
#include <QtWidgets>
#include <QTemporaryDir>
#include <memory>
#include <unistd.h>
#include <sys/stat.h>

namespace LuDash {
namespace {
bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray readFile(const QString& path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
const QString appId = QStringLiteral("org.lunadash.test.Editor.desktop");
bool selectRecorder(QDialog* chooser, bool remember) {
    auto* rememberBox = chooser->findChild<QCheckBox*>("rememberFileApplication");
    auto* systemBox = chooser->findChild<QCheckBox*>("systemFileApplication");
    auto* list = chooser->findChild<QListWidget*>("fileApplicationList");
    if (!rememberBox || !systemBox || !list) return false;
    for (auto* box : chooser->findChildren<QCheckBox*>())
        if (box != rememberBox && box != systemBox) box->setChecked(true);
    rememberBox->setChecked(remember); systemBox->setChecked(false);
    for (int row = 0; row < list->count(); ++row)
        if (list->item(row)->data(Qt::UserRole).toString() == appId) {
            if (list->item(row)->isHidden()) return false;
            list->setCurrentRow(row); return true;
        }
    return false;
}
}
class FileManagerTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void extensionRules() {
        QTemporaryDir temporary; QVERIFY(temporary.isValid());
        FileAssociations store(temporary.filePath("config/rules.json"));
        QVERIFY(!store.read().value("initialized").toBool());
        QVERIFY(store.read().value("askOnFirstOpen").toBool());
        QString error;
        QVERIFY2(store.setPreferences(true, true, &error), qPrintable(error));
        QVERIFY2(store.setRule("ext:txt", appId, "text/plain", &error), qPrintable(error));
        QCOMPARE(store.applicationForFile("report.TXT"), appId);
        QVERIFY(store.applicationForFile("report.log").isEmpty());
        QVERIFY(store.setRule("ext:user.js", appId, "text/javascript", &error));
        QCOMPARE(store.keyForFile("example.user.js"), QString("ext:user.js"));
        QCOMPARE(FileAssociations::keyForExtension("*.TXT"), QString("ext:txt"));
        QVERIFY(FileAssociations::keyForExtension("../txt").isEmpty());
        QVERIFY(!store.setRule("ext:txt", "../../run.desktop", "text/plain", &error));
        QVERIFY(!store.setRule("ext:txt", "not-installed.desktop", "text/plain", &error));
        QVERIFY(store.removeRule("ext:txt", &error));
        QVERIFY(store.applicationForFile("report.txt").isEmpty());
        QVERIFY(store.read().value("initialized").toBool());
    }
    void invalidConfigurationIsPreserved() {
        QTemporaryDir temporary;
        const auto path = temporary.filePath("file-associations.json");
        const QByteArray original("{broken JSON"); QVERIFY(writeFile(path, original));
        QString error; FileAssociations store(path);
        QVERIFY(store.read(&error).isEmpty()); QVERIFY(!error.isEmpty());
        QVERIFY(!store.setPreferences(true, false, &error));
        QCOMPARE(readFile(path), original);
    }
    void brokenConfigurationLinkIsPreserved() {
        QTemporaryDir temporary;
        const auto path = temporary.filePath("file-associations.json");
        QCOMPARE(::symlink("missing-target.json", QFile::encodeName(path).constData()), 0);
        FileAssociations store(path); QString error;
        QVERIFY(store.read(&error).isEmpty()); QVERIFY(!error.isEmpty());
        QVERIFY(!store.setPreferences(true, true, &error));
        QVERIFY(QFileInfo(path).isSymLink());
        QVERIFY(!QFileInfo::exists(temporary.filePath("missing-target.json")));
    }
    void writesCannotExceedTheReadLimit() {
        QTemporaryDir temporary;
        const auto path = temporary.filePath("file-associations.json");
        QJsonObject rules;
        for (int index = 0; index < 1650; ++index)
            rules["ext:" + QString(50, 'a') + QString::number(index)] = QJsonObject{
                {"desktopId", QString(247, 'a') + ".desktop"},
                {"mimeType", "application/" + QString(243, 'b')}};
        const QJsonDocument document(QJsonObject{{"version", 1}, {"initialized", true},
            {"askOnFirstOpen", true}, {"associations", rules}});
        const auto original = document.toJson(QJsonDocument::Compact);
        QVERIFY(original.size() < 1024 * 1024);
        QVERIFY(document.toJson(QJsonDocument::Indented).size() > 1024 * 1024);
        QVERIFY(writeFile(path, original));
        FileAssociations store(path); QString error;
        QVERIFY(!store.read(&error).isEmpty()); QVERIFY(error.isEmpty());
        QVERIFY(!store.setPreferences(true, false, &error)); QVERIFY(!error.isEmpty());
        QCOMPARE(readFile(path), original);
        QVERIFY(!store.read().isEmpty());
    }
    void handlersExcludeDispatchersAndKeepBusActivation() {
        QVERIFY(!fileApplicationAvailable("org.lunadash.test.Dispatcher.desktop"));
        QVERIFY(!fileApplicationAvailable("org.lunadash.test.NonFileApp.desktop"));
        QVERIFY(fileApplicationAvailable("org.lunadash.test.BusApp.desktop"));
        QVERIFY(fileApplicationAvailable(appId));
    }
    void legacyRulesMigrateOnlyOnce() {
        FileAssociations store;
        QFile::remove(store.path());
        QSettings settings;
        settings.remove("files/associationMigrationVersion");
        settings.setValue("fileAssociations/ext_txt", "/old/applications/" + appId);
        settings.setValue("fileAssociations/ext_log", "__system__");
        settings.sync();
        QString error;
        QVERIFY2(migrateLegacyFileAssociations(&error), qPrintable(error));
        QCOMPARE(store.applicationForFile("notes.txt"), appId);
        QCOMPARE(store.applicationForFile("app.log"), QString("__system__"));
        QVERIFY(store.removeRule("ext:txt", &error));
        QVERIFY(migrateLegacyFileAssociations(&error));
        QVERIFY(store.applicationForFile("notes.txt").isEmpty());
        QVERIFY(settings.contains("fileAssociations/ext_txt"));
    }
    void defaultOnlyChooserDoesNotLaunch() {
        QTemporaryDir temporary;
        const auto file = temporary.filePath("default-only.txt"); QVERIFY(writeFile(file, "test"));
        const auto capture = QString::fromLocal8Bit(qgetenv("LUNADASH_TEST_CAPTURE"));
        QFile::remove(capture);
        std::unique_ptr<QWidget> parent(new QWidget); parent->show();
        bool saved = false;
        chooseFileDefault(parent.get(), file, [&saved](const QString& error) { saved = error.isEmpty(); });
        auto* chooser = parent->findChild<QDialog*>("fileApplicationChooser"); QVERIFY(chooser);
        auto* apps = chooser->findChild<QListWidget*>("fileApplicationList"); QVERIFY(apps);
        auto* showAll = chooser->findChildren<QCheckBox*>().first();
        showAll->setChecked(true);
        QListWidgetItem* selected = nullptr;
        for (int row = 0; row < apps->count(); ++row)
            if (apps->item(row)->data(Qt::UserRole).toString() == appId) selected = apps->item(row);
        QVERIFY(selected); apps->setCurrentItem(selected);
        chooser->findChild<QPushButton*>("confirmFileApplication")->click();
        QVERIFY(saved);
        QCOMPARE(FileAssociations().applicationForFile(file), appId);
        QVERIFY(!QFileInfo::exists(capture));
    }
    void copyFoldersAndLinks() {
        QTemporaryDir temporary;
        QDir root(temporary.path()); QVERIFY(root.mkpath("source/folder/nested")); QVERIFY(root.mkdir("destination"));
        QVERIFY(writeFile(root.filePath("source/folder/nested/.hidden"), "contents"));
        const auto link = root.filePath("source/folder/link");
        QVERIFY(::symlink("nested/.hidden", QFile::encodeName(link).constData()) == 0);
        QVERIFY(::symlink("missing", QFile::encodeName(root.filePath("source/folder/dangling")).constData()) == 0);
        const auto error = performFileOperation("copy", {root.filePath("source/folder")}, root.filePath("destination"));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(readFile(root.filePath("destination/folder/nested/.hidden")), QByteArray("contents"));
        QVERIFY(QFileInfo(root.filePath("destination/folder/link")).isSymLink());
        QVERIFY(QFileInfo(root.filePath("destination/folder/dangling")).isSymLink());
        QVERIFY(QFileInfo::exists(root.filePath("source/folder/nested/.hidden")));
    }
    void copyRejectsOverwritesAndRecursion() {
        QTemporaryDir temporary; QDir root(temporary.path());
        QVERIFY(root.mkpath("source/folder")); QVERIFY(root.mkdir("destination"));
        QVERIFY(writeFile(root.filePath("source/file"), "source"));
        QVERIFY(writeFile(root.filePath("destination/file"), "keep"));
        QVERIFY(!performFileOperation("copy", {root.filePath("source/file")}, root.filePath("destination")).isEmpty());
        QCOMPARE(readFile(root.filePath("destination/file")), QByteArray("keep"));
        QVERIFY(!performFileOperation("copy", {root.filePath("source")}, root.filePath("source/folder")).isEmpty());
        QVERIFY(!performFileOperation("move", {root.filePath("source"), root.filePath("source/file")}, root.filePath("destination")).isEmpty());
        QVERIFY(QFileInfo::exists(root.filePath("source/file")));
    }
    void specialFilesDoNotBlock() {
        QTemporaryDir temporary; QDir root(temporary.path());
        QVERIFY(root.mkdir("destination"));
        QVERIFY(::mkfifo(QFile::encodeName(root.filePath("pipe")).constData(), 0600) == 0);
        QVERIFY(!performFileOperation("copy", {root.filePath("pipe")}, root.filePath("destination")).isEmpty());
        QVERIFY(!QFileInfo::exists(root.filePath("destination/pipe")));
        QCOMPARE(QDir(root.filePath("destination")).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
    }
    void moveWithinFilesystem() {
        QTemporaryDir temporary; QDir root(temporary.path());
        QVERIFY(root.mkdir("destination")); QVERIFY(writeFile(root.filePath("file"), "move"));
        const auto error = performFileOperation("move", {root.filePath("file")}, root.filePath("destination"));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!QFileInfo::exists(root.filePath("file")));
        QCOMPARE(readFile(root.filePath("destination/file")), QByteArray("move"));
    }
    void desktopLaunchDoesNotInterpretFileNames() {
        QTemporaryDir temporary;
        const auto file = temporary.filePath("quotes ' ; $(touch INJECTED) %.txt");
        QVERIFY(writeFile(file, "test"));
        QString error;
        QVERIFY2(launchFilesWithApplication(appId, {file}, &error), qPrintable(error));
        const auto capture = QString::fromLocal8Bit(qgetenv("LUNADASH_TEST_CAPTURE"));
        QTRY_COMPARE(readFile(capture), QFile::encodeName(file) + '\n');
        QVERIFY(!QFileInfo::exists(temporary.filePath("INJECTED")));
    }
    void firstRunAndFileTypePrompt() {
        const FileAssociations store;
        QFile::remove(store.path());
        std::unique_ptr<QWidget> parent(new QWidget); parent->show();
        showFileManagerFirstRun(parent.get());
        auto* first = parent->findChild<QDialog*>("fileManagerFirstRun"); QVERIFY(first);
        first->reject();
        QVERIFY(!store.read().value("initialized").toBool());
        QVERIFY(store.setPreferences(true, true));
        QTemporaryDir temporary;
        const auto path = temporary.filePath("unknown.txt"); QVERIFY(writeFile(path, "hello"));
        openAssociatedFiles(parent.get(), {path}, false, [](const QString&) {});
        auto* chooser = parent->findChild<QDialog*>("fileApplicationChooser"); QVERIFY(chooser);
        QVERIFY(chooser->findChild<QCheckBox*>("rememberFileApplication")->isChecked());
        QVERIFY(!chooser->findChild<QCheckBox*>("systemFileApplication")->isChecked());
        chooser->reject();
        QVERIFY(store.applicationForFile(path).isEmpty());
    }
    void openOnceAndRememberUseTheRealChooser() {
        const FileAssociations store;
        QFile::remove(store.path()); QVERIFY(store.setPreferences(true, true));
        std::unique_ptr<QWidget> parent(new QWidget); parent->show();
        QTemporaryDir temporary;
        const auto path = temporary.filePath("choice.txt"); QVERIFY(writeFile(path, "test"));
        const auto capture = QString::fromLocal8Bit(qgetenv("LUNADASH_TEST_CAPTURE"));
        for (const bool remember : {false, true}) {
            QFile::remove(capture);
            bool reported = false; QString error;
            openAssociatedFiles(parent.get(), {path}, true, [&](const QString& message) { error = message; reported = true; });
            auto* chooser = parent->findChild<QDialog*>("fileApplicationChooser"); QVERIFY(chooser);
            QVERIFY(selectRecorder(chooser, remember));
            auto* accept = chooser->findChild<QPushButton*>("confirmFileApplication"); QVERIFY(accept); QVERIFY(accept->isEnabled());
            accept->click();
            QTRY_VERIFY(reported); QVERIFY2(error.isEmpty(), qPrintable(error));
            QTRY_COMPARE(readFile(capture), QFile::encodeName(path) + '\n');
            QCOMPARE(store.applicationForFile(path), remember ? appId : QString{});
            QTRY_VERIFY(!parent->findChild<QDialog*>("fileApplicationChooser"));
        }
        QFile::remove(capture);
        bool reported = false;
        openAssociatedFiles(parent.get(), {path}, false, [&](const QString& error) { QVERIFY(error.isEmpty()); reported = true; });
        QVERIFY(!parent->findChild<QDialog*>("fileApplicationChooser"));
        QTRY_VERIFY(reported);
        QTRY_COMPARE(readFile(capture), QFile::encodeName(path) + '\n');
    }
    void contextSelectionAndClipboard() {
        QVERIFY(FileAssociations().setPreferences(true, true));
        QTemporaryDir temporary;
        const auto firstPath = temporary.filePath("first.txt"), secondPath = temporary.filePath("second.txt");
        QVERIFY(writeFile(firstPath, "one")); QVERIFY(writeFile(secondPath, "two"));
        std::unique_ptr<QWidget> page(createFileManager()); page->resize(1040, 680); page->show();
        auto* location = page->findChild<QLineEdit*>("fileLocation"); QVERIFY(location);
        location->setText(temporary.path()); QMetaObject::invokeMethod(location, "returnPressed");
        auto* icons = page->findChild<QListView*>("fileIcons"); QVERIFY(icons);
        auto* model = qobject_cast<QFileSystemModel*>(icons->model()); QVERIFY(model);
        QTRY_VERIFY(model->rowCount(icons->rootIndex()) == 2);
        const auto first = model->index(firstPath), second = model->index(secondPath);
        QVERIFY(first.isValid()); QVERIFY(second.isValid());
        icons->selectionModel()->select(first, QItemSelectionModel::ClearAndSelect);
        icons->selectionModel()->select(second, QItemSelectionModel::Select);
        icons->setCurrentIndex(first);
        icons->selectionModel()->select(second, QItemSelectionModel::Select);
        QMetaObject::invokeMethod(icons, "customContextMenuRequested", Q_ARG(QPoint, QPoint(-1, -1)));
        QPointer<QMenu> menu = page->findChild<QMenu*>("fileContextMenu"); QVERIFY(menu);
        menu->hide(); // Menus normally hide without receiving QWidget::close().
        QTRY_VERIFY(menu.isNull());
        auto* copy = page->findChild<QAction*>("filesAction_copy"); QVERIFY(copy); QVERIFY(copy->isEnabled()); copy->trigger();
        QCOMPARE(QApplication::clipboard()->mimeData()->urls().size(), 2);
        QVERIFY(page->findChild<QTreeView*>("fileView")->selectionModel() == icons->selectionModel());
        location->setFocus(); location->selectAll(); QTest::keyClick(location, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), temporary.path());
    }
};
} // namespace LuDash

int main(int argc, char** argv) {
    QTemporaryDir sandbox;
    if (!sandbox.isValid()) return 2;
    const auto config = sandbox.filePath("config"), data = sandbox.filePath("data");
    QDir().mkpath(config); QDir().mkpath(data + "/applications");
    qputenv("HOME", QFile::encodeName(sandbox.path()));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(sandbox.filePath("cache")));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(config));
    qputenv("XDG_CONFIG_DIRS", QFile::encodeName(sandbox.filePath("empty-config")));
    qputenv("XDG_DATA_HOME", QFile::encodeName(data));
    qputenv("LUNADASH_TEST_CAPTURE", QFile::encodeName(sandbox.filePath("capture")));
    const auto script = sandbox.filePath("capture.sh");
    if (!LuDash::writeFile(script, "printf '%s\\n' \"$@\" > \"$LUNADASH_TEST_CAPTURE\"\n")) return 2;
    const auto desktop = "[Desktop Entry]\nType=Application\nName=LunaDash Test Editor\nExec=/bin/sh " +
        QFile::encodeName(script) + " %F\nMimeType=text/plain;\nIcon=text-editor\n";
    if (!LuDash::writeFile(data + "/applications/" + LuDash::appId, desktop)) return 2;
    for (const auto& fixture : QList<QPair<QString, QByteArray>>{
        {"Dispatcher", "Exec=gio open %U\n"}, {"NonFileApp", "Exec=/bin/true\n"},
        {"BusApp", "Exec=/bin/true\nDBusActivatable=true\n"}}) {
        if (!LuDash::writeFile(data + "/applications/org.lunadash.test." + fixture.first + ".desktop",
            "[Desktop Entry]\nType=Application\nName=Test fixture\n" + fixture.second)) return 2;
    }
    QApplication app(argc, argv); app.setOrganizationName("LunaDash"); app.setApplicationName("FilesTest");
    LuDash::FileManagerTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "FileManagerTests.moc"
