#include <LuDash/file_associations/FileAssociations.h>
#include <LuDash/file_association_ui/FileAssociationUi.h>
#include <LuDash/file_manager/FileManager.h>
#include <LuDash/file_operations/FileOperations.h>
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
        // Restore multiple selection after changing the current index.
        icons->selectionModel()->select(second, QItemSelectionModel::Select);
        QMetaObject::invokeMethod(icons, "customContextMenuRequested", Q_ARG(QPoint, QPoint(-1, -1)));
        auto* menu = page->findChild<QMenu*>("fileContextMenu"); QVERIFY(menu); menu->close();
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
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(config));
    qputenv("XDG_CONFIG_DIRS", QFile::encodeName(sandbox.filePath("empty-config")));
    qputenv("XDG_DATA_HOME", QFile::encodeName(data));
    qputenv("LUNADASH_TEST_CAPTURE", QFile::encodeName(sandbox.filePath("capture")));
    const auto script = sandbox.filePath("capture.sh");
    if (!LuDash::writeFile(script, "printf '%s\\n' \"$@\" > \"$LUNADASH_TEST_CAPTURE\"\n")) return 2;
    const auto desktop = "[Desktop Entry]\nType=Application\nName=LunaDash Test Editor\nExec=/bin/sh " +
        QFile::encodeName(script) + " %F\nMimeType=text/plain;\nIcon=text-editor\n";
    if (!LuDash::writeFile(data + "/applications/" + LuDash::appId, desktop)) return 2;
    QApplication app(argc, argv); app.setOrganizationName("LunaDash"); app.setApplicationName("FilesTest");
    LuDash::FileManagerTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "FileManagerTests.moc"
