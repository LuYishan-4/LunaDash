#include <LuDash/configuration/DesktopPreferences.h>
#include <LuDash/network/NetworkStatus.h>
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
namespace LuDash {
class PreferenceTests final : public QObject {
    Q_OBJECT
private slots:
    void validatesBeforeWriting() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        QString error;
        const auto original = desktopPreferences();
        QVERIFY(!updateDesktopPreferences({{"accent", "#123456"}, {"gap", 99}}, &error));
        QCOMPARE(desktopPreferences(), original);
        for (const auto& invalid : {QJsonObject{{"gap", "12"}}, QJsonObject{{"gap", 4.5}}, QJsonObject{{"accent", "red; command"}},
                                  QJsonObject{{"panelHeight", -2}}, QJsonObject{{"overview", 1}}, QJsonObject{{"unknown", true}}}) {
            QVERIFY(!updateDesktopPreferences(invalid, &error));
            QCOMPARE(desktopPreferences(), original);
        }
        QVERIFY(updateDesktopPreferences({{"gap", 20}, {"panelHeight", 36}, {"accent", "#c4b5fd"}, {"overview", false}}, &error));
        QCOMPARE(desktopPreferences().value("gap").toInt(), 20);
        QCOMPARE(desktopPreferences().value("accent").toString(), QString("#c4b5fd"));
        QVERIFY(!desktopPreferences().value("overview").toBool());
        QSettings().setValue("desktop/gap", -100);
        QCOMPARE(desktopPreferences().value("gap"), original.value("gap"));
        qunsetenv("LUDASH_SKIP_SETUP");
        QVERIFY(!setupComplete());
        setSetupComplete(true); QVERIFY(setupComplete());
        setSetupComplete(false); QVERIFY(!setupComplete());
    }
    void distinguishesLinkFromInternet() {
        QVERIFY(!describeNetwork(20, 4).value("internet").toBool());
        QVERIFY(!describeNetwork(40, 0).value("connected").toBool());
        QVERIFY(describeNetwork(50, 0).value("connected").toBool());
        QVERIFY(!describeNetwork(50, 0).value("internet").toBool());
        QCOMPARE(describeNetwork(60, 2).value("label").toString(), QString("Sign-in required"));
        QVERIFY(describeNetwork(70, 4).value("internet").toBool());
    }
};
}
QTEST_GUILESS_MAIN(LuDash::PreferenceTests)
#include "PreferenceTests.moc"
