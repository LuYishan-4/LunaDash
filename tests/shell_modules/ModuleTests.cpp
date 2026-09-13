#include <LuDash/shell_modules/ModuleSchema.h>
#include <LuDash/shell_modules/ShellModules.h>
#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
namespace LuDash {
class ModuleTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QCoreApplication::setOrganizationName("LuDashTests"); QCoreApplication::setApplicationName("LuDash"); }
    void validationPreservesOutput() {
        QJsonObject result{{"sentinel", true}}; QString error;
        for (const auto& input : {R"({"schemaVersion":2,"modules":{}})", R"({"schemaVersion":1,"modules":{"panel":{"style":{"height":-1}}}})", R"({"schemaVersion":1,"modules":{"settings":{"enabled":false}}})", R"({"schemaVersion":1,"modules":{"panel":{"custom":{"enabled":true,"entry":"../Main.qml"}}}})", R"({"schemaVersion":1,"modules":{"panel":{"style":{"background":"red"}}}})"}) {
            QVERIFY(!validateModuleDocument(input, &result, &error)); QVERIFY(result.value("sentinel").toBool()); QVERIFY(!error.isEmpty());
        }
        QVERIFY(!validateModuleDocument(QByteArray(16385, ' '), &result, &error));
        QVERIFY(validateModuleDocument(R"({"schemaVersion":1,"modules":{"panel":{"style":{"height":48,"accent":"inherit","edge":"bottom"}}}})", &result, &error));
        QCOMPARE(result.value("modules").toObject().size(), 9);
    }
    void persistenceTrustAndTemplates() {
        QTemporaryDir directory; qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        ShellModules modules; QString error;
        QVERIFY(!modules.snapshot().value("trusted").toBool());
        QVERIFY(modules.installTemplate("panel", &error)); QVERIFY(!modules.installTemplate("panel", &error));
        const QByteArray json = R"({"schemaVersion":1,"modules":{"panel":{"style":{"height":48,"margin":8,"edge":"bottom"},"custom":{"enabled":true,"entry":"panel/Main.qml"}}}})";
        QVERIFY(modules.apply(json, &error)); QCOMPARE(modules.panelExtent(40), 64); QVERIFY(modules.panelAtBottom());
        auto source = [&] { return modules.snapshot().value("modules").toObject().value("panel").toObject().value("custom").toObject().value("source").toString(); };
        QVERIFY(source().isEmpty()); QVERIFY(modules.setCodeTrusted(true, &error)); QVERIFY(source().startsWith("file:"));
        QFile file(modules.snapshot().value("path").toString()); QVERIFY(file.open(QIODevice::ReadOnly)); const auto before = file.readAll(); file.close();
        QVERIFY(!modules.apply("{}", &error)); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), before); file.close();
        const auto root = modules.snapshot().value("codeRoot").toString();
        QFile outside(directory.filePath("Outside.qml")); QVERIFY(outside.open(QIODevice::WriteOnly)); outside.write("import QtQuick\nItem {}\n"); outside.close();
        QVERIFY(QFile::link(outside.fileName(), root + "/panel/Escape.qml"));
        QVERIFY(!modules.apply(QByteArray(json).replace("panel/Main.qml", "panel/Escape.qml"), &error));
        QVERIFY(modules.reset(&error)); QVERIFY(!modules.snapshot().value("trusted").toBool()); QVERIFY(QFileInfo::exists(root + "/panel/Main.qml"));
    }
};
}
QTEST_GUILESS_MAIN(LuDash::ModuleTests)
#include "ModuleTests.moc"
