#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

namespace LunaDash {
class PlayerFixture final : public QDBusVirtualObject {
public:
  QString status = "Playing";
  QString title = "Test track";
  double volume = 0.65;
  QString lastMethod;
  qint64 position = 12000000;
  bool fail = false;
  QString introspect(const QString &) const override { return {}; }
  bool handleMessage(const QDBusMessage &message,
                     const QDBusConnection &connection) override {
    if (fail) {
      connection.send(message.createErrorReply(
          "org.freedesktop.DBus.Error.Failed", "Player unavailable"));
      return true;
    }
    const auto args = message.arguments();
    if (message.member() == "GetAll") {
      QVariantMap properties;
      if (args.value(0).toString() == "org.mpris.MediaPlayer2") {
        properties = {{"Identity", "Fixture player"}, {"CanRaise", true}};
      } else {
        QVariantMap metadata{
            {"xesam:title", title},
            {"xesam:artist", QStringList{"Artist"}},
            {"mpris:trackid",
             QVariant::fromValue(QDBusObjectPath("/tracks/test"))},
            {"mpris:length", qint64(180000000)},
            {"xesam:lyrics", "[00:01.00]First line\n[00:12.00]Current line"}};
        properties = {{"PlaybackStatus", status}, {"Metadata", metadata},
                      {"Position", position},     {"Volume", volume},
                      {"Shuffle", false},         {"LoopStatus", "None"},
                      {"CanControl", true},       {"CanPlay", true},
                      {"CanPause", true},         {"CanSeek", true},
                      {"CanGoNext", true},        {"CanGoPrevious", true}};
      }
      connection.send(message.createReply(QVariantList{properties}));
    } else if (message.member() == "Set") {
      lastMethod = args.value(1).toString();
      if (lastMethod == "Volume")
        volume =
            qvariant_cast<QDBusVariant>(args.value(2)).variant().toDouble();
      connection.send(message.createReply());
    } else {
      lastMethod = message.member();
      if (lastMethod == "PlayPause")
        status = status == "Playing" ? "Paused" : "Playing";
      if (lastMethod == "SetPosition")
        position = args.value(1).toLongLong();
      connection.send(message.createReply());
    }
    return true;
  }
};

class MediaPlayerTests final : public QObject {
  Q_OBJECT
  QTemporaryDir runtime;
  QProcess sessionDaemon, userDaemon;
  QString sessionAddress;
  std::unique_ptr<QDBusConnection> pausedBus, playingBus, userBus;
  PlayerFixture paused, playing, user;

  QString startBus(QProcess &process, const QString &path) {
    process.start("dbus-daemon",
                  {"--session", "--nofork", "--address=unix:path=" + path,
                   "--print-address=1"});
    if (!process.waitForStarted(3000) || !process.waitForReadyRead(3000))
      return {};
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  }
  QJsonObject invoke(const QStringList &arguments, int expected = 0,
                     bool sameBus = false) {
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("DBUS_SESSION_BUS_ADDRESS",
                       sameBus ? "unix:path=" + runtime.filePath("bus")
                               : sessionAddress);
    environment.insert("XDG_RUNTIME_DIR", runtime.path());
    process.setProcessEnvironment(environment);
    process.start(QCoreApplication::applicationDirPath() +
                      "/lunadash-shell-tool",
                  arguments);
    if (!process.waitForStarted(2000)) {
      QTest::qFail("Could not start media helper", __FILE__, __LINE__);
      return {};
    }
    QElapsedTimer timer;
    timer.start();
    while (process.state() != QProcess::NotRunning && timer.elapsed() < 6000)
      QTest::qWait(10); // Dispatch fixture D-Bus replies while the helper runs.
    if (process.state() != QProcess::NotRunning) {
      process.kill();
      process.waitForFinished();
      QTest::qFail("Media helper timed out", __FILE__, __LINE__);
      return {};
    }
    if (process.exitCode() != expected)
      QTest::qFail(process.readAllStandardError().constData(), __FILE__,
                   __LINE__);
    return QJsonDocument::fromJson(process.readAllStandardOutput()).object();
  }
private Q_SLOTS:
  void initTestCase() {
    if (QStandardPaths::findExecutable("dbus-daemon").isEmpty())
      QSKIP("dbus-daemon is unavailable");
    QVERIFY(runtime.isValid());
    sessionAddress = startBus(sessionDaemon, runtime.filePath("private-bus"));
    const QString userAddress = startBus(userDaemon, runtime.filePath("bus"));
    QVERIFY(!sessionAddress.isEmpty());
    QVERIFY(!userAddress.isEmpty());
    pausedBus = std::make_unique<QDBusConnection>(
        QDBusConnection::connectToBus(sessionAddress, "test-paused"));
    playingBus = std::make_unique<QDBusConnection>(
        QDBusConnection::connectToBus(sessionAddress, "test-playing"));
    userBus = std::make_unique<QDBusConnection>(
        QDBusConnection::connectToBus(userAddress, "test-user"));
    paused.status = "Paused";
    paused.title = "Paused track";
    user.title = "User bus track";
    QVERIFY(pausedBus->registerService("org.mpris.MediaPlayer2.aaa"));
    QVERIFY(playingBus->registerService("org.mpris.MediaPlayer2.zzz"));
    QVERIFY(userBus->registerService("org.mpris.MediaPlayer2.sandbox"));
    QVERIFY(
        pausedBus->registerVirtualObject("/org/mpris/MediaPlayer2", &paused));
    QVERIFY(
        playingBus->registerVirtualObject("/org/mpris/MediaPlayer2", &playing));
    QVERIFY(userBus->registerVirtualObject("/org/mpris/MediaPlayer2", &user));
  }
  void discoversBothBusesAndPrioritizesPlaying() {
    const auto result = invoke({"media-status"});
    QVERIFY(result["available"].toBool());
    QCOMPARE(result["title"].toString(), QString("Test track"));
    QCOMPARE(result["players"].toArray().size(), 3);
    QCOMPARE(result["artist"].toString(), QString("Artist"));
    QCOMPARE(result["trackId"].toString(), QString("/tracks/test"));
    QVERIFY(result["lyrics"].toString().contains("Current line"));
    QCOMPARE(invoke({"media-status"}, 0, true)["players"].toArray().size(), 1);
  }
  void selectsAndControlsTheChosenPlayer() {
    const QString service = "org.mpris.MediaPlayer2.sandbox";
    QCOMPARE(invoke({"media-status", service, "user"})["title"].toString(),
             QString("User bus track"));
    auto result = invoke({"media-action", "play-pause", service, "user"});
    QCOMPARE(user.status, QString("Paused"));
    QVERIFY(!result["playing"].toBool());
    invoke({"media-action", "volume", service, "user", "0.4"});
    QCOMPARE(user.volume, 0.4);
    invoke(
        {"media-action", "seek", service, "user", "30000000", "/tracks/test"});
    QCOMPARE(user.position, qint64(30000000));
    invoke(
        {"media-action", "seek", service, "user", "40000000", "/tracks/stale"},
        2);
    QCOMPARE(user.position, qint64(30000000));
    invoke({"media-action", "volume", service, "user", "nan"}, 2);
    QCOMPARE(user.volume, 0.4);
    invoke({"media-action", "raise", service, "user"});
    QCOMPARE(user.lastMethod, QString("Raise"));
  }
  void staleOrBrokenPlayersDoNotBlockOthers() {
    playing.fail = true;
    auto result = invoke({"media-status"});
    QCOMPARE(result["players"].toArray().size(), 2);
    paused.lastMethod.clear();
    result = invoke(
        {"media-action", "next", "org.mpris.MediaPlayer2.closed", "session"},
        2);
    QVERIFY(result.contains("error"));
    QVERIFY(paused.lastMethod.isEmpty());
    playing.fail = false;
  }
  void cleanupTestCase() {
    pausedBus.reset();
    playingBus.reset();
    userBus.reset();
    for (const auto *name : {"test-paused", "test-playing", "test-user"})
      QDBusConnection::disconnectFromBus(name);
    for (auto *process : {&sessionDaemon, &userDaemon})
      if (process->state() != QProcess::NotRunning) {
        process->terminate();
        if (!process->waitForFinished(2000)) {
          process->kill();
          process->waitForFinished();
        }
      }
  }
};
} // namespace LunaDash
QTEST_GUILESS_MAIN(LunaDash::MediaPlayerTests)
#include "MediaPlayerTests.moc"
