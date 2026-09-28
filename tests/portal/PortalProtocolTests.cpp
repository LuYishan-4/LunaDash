#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtTest>

namespace {
const QString service = QStringLiteral("org.freedesktop.impl.portal.desktop.lunadash");
const QString desktop = QStringLiteral("/org/freedesktop/portal/desktop");
const QString fileInterface = QStringLiteral("org.freedesktop.impl.portal.FileChooser");

QString signature(const QString &xml, const QString &interface,
                  const QString &member, const QString &direction) {
  QXmlStreamReader reader(xml);
  QString currentInterface, result;
  bool selected = false;
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement()) {
      const auto attributes = reader.attributes();
      if (reader.name() == QLatin1String("interface"))
        currentInterface = attributes.value("name").toString();
      else if (reader.name() == QLatin1String("method") ||
               reader.name() == QLatin1String("signal"))
        selected = currentInterface == interface && attributes.value("name") == member;
      else if (selected && reader.name() == QLatin1String("arg") &&
               (direction.isEmpty() || attributes.value("direction") == direction))
        result += attributes.value("type").toString();
    } else if (reader.isEndElement()) {
      if (reader.name() == QLatin1String("interface"))
        currentInterface.clear();
      else if (reader.name() == QLatin1String("method") ||
               reader.name() == QLatin1String("signal"))
        selected = false;
    }
  }
  return reader.hasError() ? QString{} : result;
}
} // namespace

class PortalProtocolTests final : public QObject {
  Q_OBJECT
  QTemporaryDir directory_;
  QProcess backend_;
  QDBusConnection bus_ = QDBusConnection::sessionBus();

  QDBusMessage call(const QString &path, const QString &interface,
                    const QString &method, const QVariantList &arguments = {}) {
    auto message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(arguments);
    return bus_.call(message, QDBus::Block, 1000);
  }

  QString introspect(const QString &path) {
    const QDBusReply<QString> reply = call(
        path, "org.freedesktop.DBus.Introspectable", "Introspect");
    return reply.isValid() ? reply.value() : QString{};
  }

private slots:
  void init() {
    QVERIFY(directory_.isValid());
    QVERIFY(bus_.isConnected());
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_QPA_PLATFORM", "offscreen");
    environment.insert("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    environment.insert("GTK_USE_PORTAL", "1");
    environment.remove("QT_NO_XDG_DESKTOP_PORTAL");
    environment.insert("XDG_CURRENT_DESKTOP", "LunaDash");
    environment.insert("XDG_CONFIG_HOME", directory_.filePath("config"));
    environment.insert("XDG_DATA_HOME", directory_.filePath("data"));
    environment.insert("XDG_CACHE_HOME", directory_.filePath("cache"));
    environment.insert("XDG_RUNTIME_DIR", directory_.path());
    environment.insert("QT_IM_MODULE", "compose");
    backend_.setProcessEnvironment(environment);
    backend_.start(QCoreApplication::applicationDirPath() +
                   "/xdg-desktop-portal-lunadash", QStringList{});
    QVERIFY(backend_.waitForStarted(3000));
    QTRY_VERIFY_WITH_TIMEOUT(bus_.interface()->isServiceRegistered(service).value(), 8000);
  }

  void cleanup() {
    if (backend_.state() != QProcess::NotRunning) {
      backend_.terminate();
      if (!backend_.waitForFinished(3000)) {
        backend_.kill();
        backend_.waitForFinished(3000);
      }
    }
    const auto diagnostics = backend_.readAllStandardError();
    if (!diagnostics.isEmpty())
      qInfo().noquote() << diagnostics;
    if (bus_.isConnected())
      QTRY_VERIFY_WITH_TIMEOUT(!bus_.interface()->isServiceRegistered(service).value(), 3000);
  }

  void coldStartExportsCompleteInterfaces() {
    // No direct C++ calls or warm-up requests may register the missing types.
    const auto xml = introspect(desktop);
    QVERIFY2(!xml.isEmpty(), "The first introspection request must succeed");
    for (const auto &method : {"OpenFile", "SaveFile", "SaveFiles"}) {
      QCOMPARE(signature(xml, fileInterface, method, "in"), QString("osssa{sv}"));
      QCOMPARE(signature(xml, fileInterface, method, "out"), QString("ua{sv}"));
    }
    QCOMPARE(signature(xml, "org.freedesktop.impl.portal.Settings",
                       "SettingChanged", {}), QString("ssv"));
    const QDBusReply<QDBusVariant> version = call(
        desktop, "org.freedesktop.DBus.Properties", "Get", {fileInterface, "version"});
    QVERIFY2(version.isValid(), qPrintable(version.error().message()));
    QCOMPARE(version.value().variant().toUInt(), 4u);
    const QDBusReply<QDBusVariant> scheme = call(
        desktop, "org.freedesktop.impl.portal.Settings", "Read",
        {"org.freedesktop.appearance", "color-scheme"});
    QVERIFY2(scheme.isValid(), qPrintable(scheme.error().message()));
    QVERIFY(scheme.value().variant().toUInt() == 1u || scheme.value().variant().toUInt() == 2u);
  }

  void closeCancelsAcrossProcesses_data() {
    QTest::addColumn<QString>("method");
    QTest::newRow("open") << QString("OpenFile");
    QTest::newRow("save") << QString("SaveFile");
  }

  void closeCancelsAcrossProcesses() {
    QFETCH(QString, method);
    const QString path = desktop + "/request/ctest/cancel";
    const QVariantMap options{{"current_folder", directory_.path().toUtf8() + '\0'}};
    auto message = QDBusMessage::createMethodCall(service, desktop, fileInterface, method);
    message.setArguments({QVariant::fromValue(QDBusObjectPath(path)), QString{}, QString{},
                          QString("Portal cancellation regression"), options});
    QDBusPendingReply<uint, QVariantMap> reply = bus_.asyncCall(message, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(introspect(path).contains("org.freedesktop.impl.portal.Request"), 5000);
    QVERIFY(!reply.isFinished());
    const auto closed = call(path, "org.freedesktop.impl.portal.Request", "Close");
    QCOMPARE(closed.type(), QDBusMessage::ReplyMessage);
    QTRY_VERIFY_WITH_TIMEOUT(reply.isFinished(), 3000);
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QCOMPARE(reply.argumentAt<0>(), 1u);
    QVERIFY(reply.argumentAt<1>().isEmpty());
    QCOMPARE(backend_.state(), QProcess::Running);
    QVERIFY(introspect(desktop).contains(fileInterface));
  }
};

QTEST_GUILESS_MAIN(PortalProtocolTests)
#include "PortalProtocolTests.moc"
