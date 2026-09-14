#include <LuDash/default_applications/DefaultApplications.h>

#include <QJsonArray>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

namespace LuDash {
class DefaultApplicationsTests : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
    QCoreApplication::setOrganizationName("LuDashDefaultApplicationsTests");
    QCoreApplication::setApplicationName("LuDash");
  }

  void preservesSafeConfiguredCommands() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       directory.path());

    QString error;
    const QJsonArray command{"/bin/echo", "literal; $(not-expanded)"};
    QVERIFY2(setDefaultApplications({{"terminal", command}}, &error),
             qPrintable(error));
    QCOMPARE(defaultApplicationCommand("terminal", &error),
             QStringList({"/bin/echo", "literal; $(not-expanded)"}));
    QVERIFY(!setDefaultApplications(
        {{"terminal", QJsonArray{"ludashctl", "launch-default", "terminal"}}},
        &error));
  }
};
} // namespace LuDash

QTEST_GUILESS_MAIN(LuDash::DefaultApplicationsTests)
#include "DefaultApplicationsTests.moc"
