#include <LuDash/audio_settings/AudioSettings.h>
#include <LuDash/power_settings/PowerSettings.h>
#include <LuDash/process_runner/CommandRunner.h>
#include <LuDash/system_tools/SystemTools.h>
#include <QtTest>
#include <QJsonArray>
namespace LuDash {
class SettingsTests final : public QObject {
    Q_OBJECT
private slots:
    void audioValidation() {
        const auto volume = parseAudioVolume("Volume: 0.45 [MUTED]\n");
        QCOMPARE(volume.value("volume").toInt(), 45); QVERIFY(volume.value("muted").toBool());
        QVERIFY(!parseAudioVolume("Volume: NaN").value("available").toBool());
        QVERIFY(!parseAudioVolume("arbitrary output").value("available").toBool());
        QVERIFY(audioCommand({{"device", "output"}, {"volume", 101}}).isEmpty());
        QVERIFY(audioCommand({{"device", "output"}, {"volume", 50}, {"extra", true}}).isEmpty());
        QVERIFY(audioCommand({{"device", "@OTHER_DEVICE@"}, {"mute", true}}).isEmpty());
        QVERIFY(audioCommand({{"device", "input"}, {"volume", "50%; id"}}).isEmpty());
        QCOMPARE(audioCommand({{"device", "output"}, {"volume", 50}}), QStringList({"set-volume", "@DEFAULT_AUDIO_SINK@", "50%", "--limit", "1.0"}));
    }
    void profileAndToolValidation() {
        const auto profiles = parsePowerProfiles("  power-saver:\n    Driver: test\n* balanced:\n  performance:\n");
        QCOMPARE(profiles.value("current").toString(), QString("balanced"));
        QCOMPARE(profiles.value("profiles").toArray().size(), 3);
        QVERIFY(!parsePowerProfiles("* arbitrary:\n").value("available").toBool());
        QVERIFY(systemSettingsCommand("network; arbitrary-command").isEmpty());
        QVERIFY(systemSettingsCommand("/bin/sh").isEmpty());
    }
    void commandOutputAndTimeoutAreBounded() {
        CommandRunner runner; bool completed = false, success = false;
        QVERIFY(runner.run("/usr/bin/printf", {"ready"}, [&](bool ok, const QByteArray& output) { success = ok && output == "ready"; completed = true; }));
        QVERIFY(!runner.run("/usr/bin/printf", {"other"}, {}));
        QTRY_VERIFY_WITH_TIMEOUT(completed, 1000); QVERIFY(success);
        completed = false;
        QVERIFY(runner.run("/usr/bin/yes", {"bounded"}, [&](bool ok, const QByteArray& output) { success = ok; QVERIFY(output.size() <= 65536); completed = true; }));
        QTRY_VERIFY_WITH_TIMEOUT(completed, 4000); QVERIFY(!success);
        completed = false;
        QVERIFY(runner.run("/usr/bin/sleep", {"10"}, [&](bool ok, const QByteArray&) { success = ok; completed = true; }));
        QTRY_VERIFY_WITH_TIMEOUT(completed, 4000); QVERIFY(!success);
    }
};
}
QTEST_GUILESS_MAIN(LuDash::SettingsTests)
#include "SettingsTests.moc"
