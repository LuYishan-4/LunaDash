#include <LuDash/session_actions/SessionActions.h>

#include <QtTest>

namespace LuDash {
class SessionActionsTests final : public QObject {
  Q_OBJECT

private slots:
  void validatesStrictAllowlist() {
    QVERIFY(isValidSessionAction("suspend"));
    QVERIFY(isValidSessionAction("reboot"));
    QVERIFY(isValidSessionAction("poweroff"));
    QVERIFY(!isValidSessionAction("logout"));
    QVERIFY(!isValidSessionAction("hibernate"));
    QVERIFY(!isValidSessionAction("reboot; arbitrary-command"));
    QVERIFY(!isValidSessionAction("/sbin/poweroff"));
    QVERIFY(!isValidSessionAction(""));
  }
};
} // namespace LuDash

QTEST_GUILESS_MAIN(LuDash::SessionActionsTests)
#include "SessionActionsTests.moc"
