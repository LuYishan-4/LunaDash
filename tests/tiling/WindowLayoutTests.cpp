#include "compositor/tiling/TilingGeometry.h"
#include "compositor/tiling/TilingLayout.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <climits>

namespace LunaDash {
class WindowLayoutTests final : public QObject {
  Q_OBJECT
  const QRect area{10, 40, 1440, 900};
  static QMap<TilingWindowId, QRect>
  geometries(const QList<TilingColumnSnapshot> &placements) {
    QMap<TilingWindowId, QRect> result;
    for (const auto &slot : placements)
      if (!slot.minimized)
        result[slot.window] = slot.geometry;
    return result;
  }
  static void verifyNoOverlap(const QList<TilingColumnSnapshot> &placements) {
    for (const auto &a : placements) {
      if (a.minimized)
        continue;
      QVERIFY(a.geometry.isValid());
      for (const auto &b : placements)
        if (!b.minimized && a.window != b.window)
          QVERIFY(!a.geometry.intersects(b.geometry));
    }
  }
private Q_SLOTS:
  void initTestCase() {
    QCoreApplication::setOrganizationName("LunaDashTests");
    QCoreApplication::setApplicationName("WindowLayout");
    QStandardPaths::setTestModeEnabled(true);
  }
  void eightRowsAndOverflow() {
    ScrollableTilingLayout layout;
    for (int i = 1; i <= 9; ++i)
      QVERIFY(layout.insert(0, i));
    auto placements = layout.layout(0, area);
    for (int i = 0; i < 9; ++i)
      QCOMPARE(placements[i].columnIndex, i);
    for (int i = 2; i <= 8; ++i)
      QVERIFY(layout.groupWith(i, i - 1));
    placements = layout.layout(0, area);
    QCOMPARE(placements.size(), 9);
    verifyNoOverlap(placements);
    for (int i = 0; i < 8; ++i) {
      QCOMPARE(placements[i].columnIndex, 0);
      QCOMPARE(placements[i].rowIndex, i);
    }
    QCOMPARE(placements[8].columnIndex, 1);
    QCOMPARE(placements[7].geometry.bottom(), area.bottom());
    QVERIFY(!layout.groupWith(9, 1));
    QVERIFY(layout.setMinimized(4, true));
    QVERIFY(!layout.groupWith(9, 1)); // Minimized clients retain their slot.
    verifyNoOverlap(layout.layout(0, area));
    QVERIFY(layout.setMinimized(4, false));
    QVERIFY(layout.remove(4));
    QVERIFY(layout.groupWith(9, 1));
    placements = layout.layout(0, area);
    QCOMPARE(placements.size(), 8);
    for (const auto &slot : placements)
      QCOMPARE(slot.columnIndex, 0);
    verifyNoOverlap(placements);
  }
  void swapsPreserveSlots() {
    ScrollableTilingLayout layout;
    for (int i = 1; i <= 3; ++i)
      layout.insert(0, i);
    layout.groupWith(2, 1);
    layout.groupWith(3, 2);
    layout.layout(0, area);
    QVERIFY(layout.resizeHeight(2, 500));
    auto before = geometries(layout.layout(0, area));
    QVERIFY(layout.swapWindows(1, 2));
    auto after = geometries(layout.layout(0, area));
    QCOMPARE(after[1], before[2]);
    QCOMPARE(after[2], before[1]);
    QCOMPARE(after[3], before[3]);
    QVERIFY(layout.expel(3));
    QVERIFY(layout.insertBeside(3, 1, false));
    auto placements = layout.layout(0, area);
    QCOMPARE(placements[1].window, TilingWindowId(3));
    QCOMPARE(placements[2].window, TilingWindowId(1));
    verifyNoOverlap(placements);
  }
  void resizingAndSingleMovement() {
    ScrollableTilingLayout layout;
    layout.insert(0, 1);
    const auto before = layout.layout(0, area).first().geometry;
    QVERIFY(layout.moveSingle(1, QPoint(60, 30), area));
    const auto moved = layout.layout(0, area).first().geometry;
    QCOMPARE(moved.size(), before.size());
    QCOMPARE(moved.topLeft(), before.topLeft() + QPoint(60, 30));
    QVERIFY(layout.resizeHeight(1, 400));
    QCOMPARE(layout.layout(0, area).first().geometry.height(), 400);
    QCOMPARE(layout.layout(0, area).first().geometry.topLeft(),
             moved.topLeft());
    QVERIFY(layout.resizeHeight(1, 700));
    QCOMPARE(layout.layout(0, area).first().geometry.height(), 700);
    layout.insert(0, 2);
    layout.insert(0, 3);
    layout.groupWith(2, 1);
    layout.groupWith(3, 2);
    verifyNoOverlap(layout.layout(0, area));
    QVERIFY(!layout.moveSingle(1, QPoint(10, 10), area));
    for (const int height : {600, 50, 10000, 300}) {
      QVERIFY(layout.resizeHeight(2, height));
      auto placements = layout.layout(0, area);
      verifyNoOverlap(placements);
      QCOMPARE(placements[1].window, TilingWindowId(2));
      QCOMPARE(placements.last().geometry.bottom(), area.bottom());
      if (height == 600 || height == 300)
        QCOMPARE(placements[1].geometry.height(), height);
    }
    QVERIFY(layout.moveToWorkspace(2, 1));
    QCOMPARE(layout.layout(0, area).size(), 2);
    QCOMPARE(layout.layout(1, area).size(), 1);
  }
  void workspaceMaximizePreservesTiling() {
    ScrollableTilingLayout layout(450, 6);
    for (int id = 1; id <= 4; ++id)
      QVERIFY(layout.insert(0, id));
    QVERIFY(layout.groupWith(2, 1));
    QVERIFY(layout.insert(1, 10, 500));
    layout.layout(0, area);
    QVERIFY(layout.resizeHeight(2, 300));
    QVERIFY(layout.focus(2));
    const auto tiled = geometries(layout.layout(0, area));
    const auto otherWorkspace = geometries(layout.layout(1, area));
    QVERIFY(layout.setMaximized(2, true));
    const auto expanded = layout.presentation(0, area);
    QCOMPARE(expanded.size(), 4);
    for (const auto &slot : expanded) {
      QCOMPARE(slot.hiddenByMaximize, slot.window != 2);
      QVERIFY(!slot.minimized);
      if (slot.window == 2)
        QCOMPARE(slot.geometry, area);
    }
    QCOMPARE(geometries(layout.layout(0, area)), tiled);
    QCOMPARE(geometries(layout.presentation(1, area)), otherWorkspace);
    QVERIFY(layout.setMaximized(2, false));
    const auto restored = layout.presentation(0, area);
    QCOMPARE(geometries(restored), tiled);
    for (const auto &slot : restored)
      QVERIFY(!slot.hiddenByMaximize);
    verifyNoOverlap(restored);
    QVERIFY(layout.setMaximized(3, true));
    QVERIFY(layout.setMaximized(1, true));
    QCOMPARE(layout.snapshot(0).maximizedWindow, TilingWindowId(1));
    for (const auto &slot : layout.presentation(0, area))
      QCOMPARE(slot.hiddenByMaximize, slot.window != 1);
    QVERIFY(layout.setMinimized(4, true));
    QVERIFY(layout.setMinimized(1, true));
    QCOMPARE(layout.snapshot(0).maximizedWindow, TilingWindowId(0));
    QVERIFY(!layout.setMaximized(1, true));
    QVERIFY(layout.setMinimized(1, false));
    QVERIFY(layout.setMaximized(2, true));
    QVERIFY(layout.remove(2));
    QCOMPARE(layout.snapshot(0).maximizedWindow, TilingWindowId(0));
    QVERIFY(layout.setMaximized(3, true));
    QVERIFY(layout.moveToWorkspace(3, 1));
    QCOMPARE(layout.snapshot(0).maximizedWindow, TilingWindowId(0));
    for (const auto &slot : layout.presentation(0, area)) {
      QVERIFY(!slot.hiddenByMaximize);
      if (slot.window == 4)
        QVERIFY(slot.minimized);
    }
  }
  void geometryBounds() {
    LuDashRectangle output[9];
    for (int count = 1; count <= 8; ++count) {
      const LuDashRectangle small{0, 0, 120, count};
      QCOMPARE(ludash_layout_column_windows(small, count, 100, output, 9),
               size_t(count));
      for (int i = 0; i < count; ++i) {
        QCOMPARE(output[i].height, 1);
        QCOMPARE(output[i].y, i);
      }
    }
    QCOMPARE(ludash_layout_column_windows({0, 0, 100, 100}, 9, 4, output, 9),
             size_t(0));
    int invalid[] = {INT_MAX, 0};
    QCOMPARE(ludash_layout_weighted_column_windows({0, 0, 100, 100}, invalid, 2,
                                                   4, output, 9),
             size_t(0));
  }
  void selectionLifecycle() {
    QTemporaryDir runtime;
    WindowSwitcher switcher;
    const QString channel = runtime.filePath("interaction.json");
    switcher.setChannelPath(channel);
    QProcessEnvironment environment;
    environment.insert("XDG_RUNTIME_DIR", runtime.path());
    environment.insert("WAYLAND_DISPLAY", runtime.filePath("no-display"));
    QJsonArray workspaces;
    for (int i = 1; i <= 10; ++i)
      workspaces.append(QJsonObject{
          {"id", i}, {"windows", QJsonArray{QJsonObject{{"id", i * 10}}}}});
    QVERIFY(switcher.begin(workspaces, 1, 1, environment));
    const int serial = switcher.serial();
    QVERIFY(switcher.begin(workspaces, 1, 1, environment));
    QCOMPARE(switcher.serial(), serial);
    switcher.step(-1);
    QCOMPARE(switcher.finish(true), 2);
    QVERIFY(switcher.begin(workspaces, 1, -1, environment));
    QCOMPARE(switcher.finish(true), 10);
    switcher.begin(workspaces, 1, 5, environment);
    QCOMPARE(switcher.finish(true), 6);
    switcher.begin(workspaces, 1, 1, environment);
    switcher.remove(20);
    QCOMPARE(switcher.snapshot().value("workspaces").toArray().size(), 10);
    QVERIFY(switcher.snapshot()
                .value("workspaces")
                .toArray()[1]
                .toObject()
                .value("windows")
                .toArray()
                .isEmpty());
    QVERIFY(switcher.select(10));
    QVERIFY(!switcher.select(11));
    QCOMPARE(switcher.finish(false), 0);
    QTest::qWait(30);
    QFile file(channel);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(!QJsonDocument::fromJson(file.readAll())
                 .object()
                 .value("active")
                 .toBool());
    QVERIFY(!(file.permissions() & QFileDevice::ReadOther));
    QTest::qWait(
        50); // Reap only our failed captures; no compositor is started.
  }
  void terminalAndShortcutMigration() {
    QTemporaryDir config;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       config.path());
    QSettings().clear();
    QCOMPARE(defaultApplicationCommand("terminal", nullptr),
             QStringList{"kitty"});
    ShortcutSettings settings;
    QCOMPARE(settings.actionFor(XKB_KEY_t, ShortcutMeta),
             QString("launchTerminal"));
    QCOMPARE(settings.actionFor(XKB_KEY_0, ShortcutMeta),
             QString("workspace10"));
    QCOMPARE(settings.actionFor(XKB_KEY_Return, ShortcutMeta),
             QString("launchTerminalAlternate"));
    QString error;
    QVERIFY(!settings.apply({{"closeWindow", "Alt+Tab"}}, &error));
    QSettings().setValue("shortcuts/bindings",
                         QJsonObject{{"launchTerminal", "Meta+Return"},
                                     {"closeWindow", "Meta+T"},
                                     {"toggleFloating", "Meta+Space"}}
                             .toVariantMap());
    ShortcutSettings migrated;
    QCOMPARE(migrated.actionFor(XKB_KEY_t, ShortcutMeta),
             QString("closeWindow"));
    QCOMPARE(migrated.actionFor(XKB_KEY_Return, ShortcutMeta),
             QString("launchTerminal"));
  }
};
} // namespace LunaDash
QTEST_GUILESS_MAIN(LunaDash::WindowLayoutTests)
#include "WindowLayoutTests.moc"
