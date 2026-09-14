#include <LuDash/window_rules/WindowRules.h>

#include <QtTest>

namespace LuDash {
class WindowRulesTests : public QObject {
  Q_OBJECT

private slots:
  void maximizesKittyTerminals() {
    QVERIFY(initialWindowPolicy("kitty", {}).maximized);
    QVERIFY(initialWindowPolicy("kitty.lunadah", {}).maximized);
    QVERIFY(initialWindowPolicy("org.example.Terminal", "LunaDah Terminal")
                .maximized);
  }

  void resolvesApplicationIcons() {
    QCOMPARE(windowIconName("lunadah-app", "LunaDah · files"),
             QString("system-file-manager"));
    QCOMPARE(windowIconName("lunadah-app", "LunaDah · settings"),
             QString("preferences-system"));
    QCOMPARE(windowIconName("kitty", "Terminal"), QString("kitty"));
    QCOMPARE(windowIconName("org.example.Editor", "Document"),
             QString("org.example.Editor"));
  }

  void leavesGenericWindowsUnforced() {
    QVERIFY(!initialWindowPolicy("org.example.Editor", "Document").maximized);
    QVERIFY(!initialWindowPolicy("org.example.Dialog", "Open Image").maximized);
    QVERIFY(
        !initialWindowPolicy("lunadah-image-picker", "Choose image").maximized);
    QVERIFY(!initialWindowPolicy("org.example.Terminal", "Terminal").maximized);
  }
};
} // namespace LuDash

QTEST_GUILESS_MAIN(LuDash::WindowRulesTests)
#include "WindowRulesTests.moc"
