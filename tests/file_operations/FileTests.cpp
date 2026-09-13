#include <LuDash/file_operations/FileOperations.h>
#include <LuDash/default_applications/DefaultApplications.h>
#include <QtTest>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QSettings>
namespace LuDash {
class FileTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QCoreApplication::setOrganizationName("LuDashTests"); QCoreApplication::setApplicationName("LuDash"); }
    void copyAndMovePreserveExistingFiles() {
        QTemporaryDir dir; QDir().mkdir(dir.filePath("target"));
        QFile source(dir.filePath("source.txt")); QVERIFY(source.open(QIODevice::WriteOnly)); source.write("original"); source.close();
        QVERIFY(performFileOperation("copy", {source.fileName()}, dir.filePath("target")).isEmpty());
        QVERIFY(!performFileOperation("copy", {source.fileName()}, dir.filePath("target")).isEmpty());
        QVERIFY(!performFileOperation("move", {source.fileName()}, dir.filePath("target")).isEmpty()); QVERIFY(source.exists());
        QFile target(dir.filePath("target/source.txt")); QVERIFY(target.open(QIODevice::ReadOnly)); QCOMPARE(target.readAll(), QByteArray("original"));
        QVERIFY(!validFileName("../escape")); QVERIFY(!validFileName("..")); QVERIFY(validFileName("Report 2026.txt"));
        QVERIFY(!performFileOperation("copy", {dir.filePath("target")}, dir.path()).isEmpty());
    }
    void defaultsValidateBeforeWriting() {
        QTemporaryDir dir; QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
        QString error; const auto before = defaultApplications();
        QVERIFY(!setDefaultApplications({{"files", QJsonArray{"ludash-desktop", "--app", "files"}}}, &error));
        QVERIFY(!setDefaultApplications({{"terminal", "foot; id"}}, &error));
        QVERIFY(!setDefaultApplications({{"files", QJsonArray{"ludash-command-that-does-not-exist"}}}, &error));
        QCOMPARE(defaultApplications(), before);
        QVERIFY(setDefaultApplications({{"terminal", QJsonArray{"/bin/echo", "literal; $(text)"}}}, &error));
        QCOMPARE(defaultApplicationCommand("terminal", &error), QStringList({"/bin/echo", "literal; $(text)"}));
        QVERIFY(setDefaultApplications({{"terminal", QJsonArray{}}}, &error));
    }
};
}
QTEST_GUILESS_MAIN(LuDash::FileTests)
#include "FileTests.moc"
