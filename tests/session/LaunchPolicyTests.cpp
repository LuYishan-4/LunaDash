#include "compositor/session/LaunchPolicy.hpp"

#include <QtTest>

using namespace LunaDash;

class LaunchPolicyTests : public QObject {
  Q_OBJECT

private slots:
  void desktopIdentityRequestsX11Helper() {
    const auto identity = identifyLaunch(
        "com.discordapp.Discord.desktop",
        {"flatpak", "run", "com.discordapp.Discord"});
    QCOMPARE(identity.desktopId, QString("com.discordapp.Discord"));
    QCOMPARE(identity.flatpakId, QString("com.discordapp.Discord"));
    QVERIFY(resolveLaunchCapabilities(
                "com.discordapp.Discord.desktop",
                {"flatpak", "run", "com.discordapp.Discord"})
                .x11Helper);
  }

  void flatpakIdentityWorksWithoutDesktopMetadata() {
    QVERIFY(resolveLaunchCapabilities(
                {}, {"flatpak", "run", "--branch=stable",
                     "com.discordapp.Discord"})
                .x11Helper);
  }

  void arbitraryArgumentsDoNotImpersonateApplications() {
    QVERIFY(!resolveLaunchCapabilities(
                 "org.example.Viewer.desktop",
                 {"viewer", "https://example.test/com.discordapp.Discord"})
                 .x11Helper);
  }

  void unknownApplicationStaysWaylandOnly() {
    QVERIFY(!resolveLaunchCapabilities(
                 "org.example.Native.desktop",
                 {"native-app", "--ozone-platform=wayland"})
                 .x11Helper);
  }
};

QTEST_APPLESS_MAIN(LaunchPolicyTests)
#include "LaunchPolicyTests.moc"
