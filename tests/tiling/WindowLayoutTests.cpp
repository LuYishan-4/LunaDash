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
  static QMap<LayoutWindowId, QRect>
  geometries(const QList<WindowPlacement> &placements) {
    QMap<LayoutWindowId, QRect> result;
    for (const auto &slot : placements)
      if (!slot.minimized)
        result[slot.window] = slot.geometry;
    return result;
  }
  static void verifyNoOverlap(const QList<WindowPlacement> &placements) {
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
  void layoutTemplateContract() {
    const auto templates = windowLayoutTemplates();
    QCOMPARE(templates.size(), 2);
    QVERIFY(templates[0].implemented);
    QVERIFY(templates[1].implemented);
    QVERIFY(createWindowLayout(WindowLayoutMode::Stacking));
    auto layout = createWindowLayout(WindowLayoutMode::Tiling);
    QVERIFY(layout);
    QCOMPARE(layout->mode(), WindowLayoutMode::Tiling);
    QVERIFY(layout->insert(0, 1));
    QVERIFY(layout->insert(0, 2));
    verifyNoOverlap(layout->layout(0, area));
    QVERIFY(layout->setMaximized(2, true));
    QCOMPARE(geometries(layout->presentation(0, area))[2], area);
    QVERIFY(layout->setMaximized(2, false));
    verifyNoOverlap(layout->presentation(0, area));
  }
  void stackingRetainsIndependentGeometry() {
    auto layout = createWindowLayout(WindowLayoutMode::Stacking);
    QVERIFY(layout->insert(0, 1, QSize(800, 500)));
    QVERIFY(layout->insert(0, 2, QSize(800, 500)));
    auto initial = geometries(layout->layout(0, area));
    QVERIFY(initial[1].intersects(initial[2]));
    QVERIFY(layout->moveSingle(1, QPoint(-100, 80), area));
    QVERIFY(layout->resize(1, 650));
    QVERIFY(layout->resizeHeight(1, 450));
    const auto resized = geometries(layout->layout(0, area));
    QCOMPARE(resized[1].size(), QSize(650, 450));
    QCOMPARE(resized[2], initial[2]);
    QVERIFY(layout->setMaximized(1, true));
    const auto maximized = layout->presentation(0, area);
    QCOMPARE(geometries(maximized)[1], area);
    for (const auto &window : maximized)
      QVERIFY(!window.hiddenByMaximize);
    QVERIFY(layout->setMaximized(1, false));
    QCOMPARE(geometries(layout->presentation(0, area)), resized);
    QVERIFY(layout->focus(1));
    QCOMPARE(layout->snapshot(0).columns.last().window, LayoutWindowId(1));
    QVERIFY(layout->setMinimized(1, true));
    QCOMPARE(layout->snapshot(0).focusedWindow, LayoutWindowId(2));
    QVERIFY(layout->setMinimized(1, false));
    QCOMPARE(geometries(layout->layout(0, area)), resized);
    QVERIFY(layout->moveToWorkspace(1, 1));
    QCOMPARE(layout->snapshot(0).columns.size(), 1);
    QCOMPARE(layout->snapshot(1).columns.size(), 1);
    QVERIFY(layout->moveSingle(1, QPoint(100000, -100000), area));
    for (const auto &window : layout->layout(1, QRect(0, 0, 300, 200)))
      QVERIFY(QRect(0, 0, 300, 200).contains(window.geometry));
    QVERIFY(layout->remove(1));
    QCOMPARE(layout->snapshot(1).focusedWindow, LayoutWindowId(0));
  }
  void fixedSplitsStayOnScreen() {
    TilingLayout layout(960, 6);
    QVERIFY(layout.insert(0, 1, QSize(900, 600)));
    QCOMPARE(layout.layout(0, area).first().geometry.size(), QSize(900, 600));
    QVERIFY(layout.insert(0, 2));
    auto two = geometries(layout.layout(0, area));
    QCOMPARE(two[1].height(), area.height());
    QCOMPARE(two[2].height(), area.height());
    QCOMPARE(two[1].right() + 7, two[2].left());
    QVERIFY(layout.insert(0, 3));
    auto three = geometries(layout.layout(0, area));
    QCOMPARE(three[1], two[1]);
    QCOMPARE(three[2].left(), two[2].left());
    QCOMPARE(three[3].left(), two[2].left());
    QCOMPARE(three[2].bottom() + 7, three[3].top());
    QVERIFY(layout.focus(1));
    const auto unchanged = geometries(layout.layout(0, area));
    QCOMPARE(unchanged, three); // Focusing never scrolls or rearranges slots.
    QVERIFY(layout.focusRight(0));
    QVERIFY(layout.focusDown(0));
    QCOMPARE(layout.snapshot(0).focusedWindow, LayoutWindowId(3));
    QVERIFY(layout.resize(3, 900));
    QVERIFY(layout.resizeHeight(3, 600));
    auto resized = layout.layout(0, area);
    verifyNoOverlap(resized);
    for (const auto &slot : resized)
      QVERIFY(area.contains(slot.geometry));
    const auto beforeMinimize = geometries(resized);
    QVERIFY(layout.setMinimized(2, true));
    verifyNoOverlap(layout.layout(0, area));
    QVERIFY(layout.setMinimized(2, false));
    QCOMPARE(geometries(layout.layout(0, area)), beforeMinimize);
    QVERIFY(layout.remove(3));
    QCOMPARE(geometries(layout.layout(0, area))[2].height(), area.height());
  }
  void manyWindowsAndOutputChanges() {
    for (const QRect bounds :
         {area, QRect(-800, 20, 800, 1200), QRect(0, 0, 320, 240)}) {
      TilingLayout layout(960, 6);
      for (int id = 1; id <= 64; ++id) {
        QVERIFY(layout.insert(0, id));
        const auto placements = layout.layout(0, bounds);
        QCOMPARE(placements.size(), id);
        verifyNoOverlap(placements);
        for (const auto &slot : placements)
          QVERIFY(bounds.contains(slot.geometry));
      }
      for (const QRect resized : {QRect(0, 0, 500, 300), area}) {
        const auto placements = layout.layout(0, resized);
        QCOMPARE(placements.size(), 64);
        verifyNoOverlap(placements);
        for (const auto &slot : placements)
          QVERIFY(resized.contains(slot.geometry));
      }
    }
  }
  void mixedLayoutEditsRemainBounded() {
    TilingLayout layout(960, 6);
    for (int id = 1; id <= 16; ++id)
      layout.insert(0, id);
    quint32 random = 12345;
    for (int step = 0; step < 400; ++step) {
      random = random * 1664525U + 1013904223U;
      const int id = 1 + (random >> 8) % 16;
      const int target = 1 + (random >> 16) % 16;
      switch (step % 8) {
      case 0:
        layout.groupWith(id, target);
        break;
      case 1:
        layout.expel(id);
        break;
      case 2:
        layout.swapWindows(id, target);
        break;
      case 3:
        layout.setMinimized(id, true);
        break;
      case 4:
        layout.setMinimized(id, false);
        break;
      case 5:
        layout.moveToWorkspace(id, (random >> 24) % 2);
        break;
      case 6:
        layout.resize(id, 120 + target * 60);
        break;
      case 7:
        layout.resizeHeight(id, 80 + target * 40);
        break;
      }
      int total = 0;
      for (int workspace = 0; workspace < 2; ++workspace) {
        const auto placements = layout.layout(workspace, area);
        total += placements.size();
        verifyNoOverlap(placements);
        for (const auto &slot : placements)
          if (!slot.minimized)
            QVERIFY(area.contains(slot.geometry));
      }
      QCOMPARE(total, 16);
    }
  }
  void singleWindowProportionsAndBounds() {
    TilingLayout layout;
    layout.insert(0, 1, QSize(2400, 1600));
    const auto fitted = layout.layout(0, area).first().geometry;
    QVERIFY(area.contains(fitted));
    QCOMPARE(fitted.width() * 2, fitted.height() * 3);
    QVERIFY(layout.moveSingle(1, QPoint(10000, -10000), area));
    QVERIFY(area.contains(layout.layout(0, area).first().geometry));
    QVERIFY(layout.resize(1, 10000));
    QVERIFY(layout.resizeHeight(1, 10000));
    QCOMPARE(layout.layout(0, area).first().geometry, area);
  }
  void eightRowsAndOverflow() {
    TilingLayout layout;
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
    TilingLayout layout;
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
    QCOMPARE(placements[1].window, LayoutWindowId(3));
    QCOMPARE(placements[2].window, LayoutWindowId(1));
    verifyNoOverlap(placements);
  }
  void resizingAndSingleMovement() {
    TilingLayout layout;
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
      QCOMPARE(placements[1].window, LayoutWindowId(2));
      QCOMPARE(placements.last().geometry.bottom(), area.bottom());
      if (height == 600 || height == 300)
        QCOMPARE(placements[1].geometry.height(), height);
    }
    QVERIFY(layout.moveToWorkspace(2, 1));
    QCOMPARE(layout.layout(0, area).size(), 2);
    QCOMPARE(layout.layout(1, area).size(), 1);
  }
  void workspaceMaximizePreservesTiling() {
    TilingLayout layout(450, 6);
    for (int id = 1; id <= 4; ++id)
      QVERIFY(layout.insert(0, id));
    QVERIFY(layout.groupWith(2, 1));
    QVERIFY(layout.insert(1, 10, QSize(500, 400)));
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
    QCOMPARE(layout.snapshot(0).maximizedWindow, LayoutWindowId(1));
    for (const auto &slot : layout.presentation(0, area))
      QCOMPARE(slot.hiddenByMaximize, slot.window != 1);
    QVERIFY(layout.setMinimized(4, true));
    QVERIFY(layout.setMinimized(1, true));
    QCOMPARE(layout.snapshot(0).maximizedWindow, LayoutWindowId(0));
    QVERIFY(!layout.setMaximized(1, true));
    QVERIFY(layout.setMinimized(1, false));
    QVERIFY(layout.setMaximized(2, true));
    QVERIFY(layout.remove(2));
    QCOMPARE(layout.snapshot(0).maximizedWindow, LayoutWindowId(0));
    QVERIFY(layout.setMaximized(3, true));
    QVERIFY(layout.moveToWorkspace(3, 1));
    QCOMPARE(layout.snapshot(0).maximizedWindow, LayoutWindowId(0));
    for (const auto &slot : layout.presentation(0, area)) {
      QVERIFY(!slot.hiddenByMaximize);
      if (slot.window == 4)
        QVERIFY(slot.minimized);
    }
  }
  void geometryBounds() {
    LuDashRectangle halves[2];
    for (const int vertical : {0, 1}) {
      QVERIFY(ludash_split_rectangle({-10, 20, 31, 29}, vertical, 500000, 6, 1,
                                     1, halves));
      const auto a =
          QRect(halves[0].x, halves[0].y, halves[0].width, halves[0].height);
      const auto b =
          QRect(halves[1].x, halves[1].y, halves[1].width, halves[1].height);
      QVERIFY(!a.intersects(b));
      QVERIFY(QRect(-10, 20, 31, 29).contains(a));
      QVERIFY(QRect(-10, 20, 31, 29).contains(b));
    }
    QVERIFY(ludash_split_rectangle({0, 0, 2, 8}, 1, 0, INT_MAX, 1, 1, halves));
    QCOMPARE(halves[0].width, 1);
    QCOMPARE(halves[1].x, 1);
    QVERIFY(!ludash_split_rectangle({0, 0, 1, 8}, 1, 500000, 0, 1, 1, halves));
    QVERIFY(!ludash_split_rectangle({INT_MAX, 0, 10, 8}, 1, 500000, 0, 1, 1,
                                    halves));
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
