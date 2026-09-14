#include <LuDash/window_rules/WindowRules.h>

#include <QtTest>

namespace LuDash {
class WindowRulesTests : public QObject {
  Q_OBJECT

private slots:

  void resolvesApplicationIcons() {
    QCOMPARE(windowIconName("lunadah-app", "LunaDah · files"),
             QString("system-file-manager"));
    QCOMPARE(windowIconName("lunadah-app", "LunaDah · settings"),
             QString("preferences-system"));
    QCOMPARE(windowIconName("kitty", "Terminal"), QString("kitty"));
    QCOMPARE(windowIconName("org.example.Editor", "Document"),
             QString("org.example.Editor"));
  }

  void windowsOpenAsIndependentColumns() {
    QVERIFY(!initialWindowPolicy("org.example.Editor", "Document").maximized);
    QVERIFY(!initialWindowPolicy("kitty", "Terminal").maximized);
    QVERIFY(
        !initialWindowPolicy("lunadah-image-picker", "Choose image").maximized);
  }
};
} // namespace LuDash

QTEST_GUILESS_MAIN(LuDash::WindowRulesTests)
#include "WindowRulesTests.moc"
