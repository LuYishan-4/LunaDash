#include "compositor/tiling/TilingGeometry.h"
#include "compositor/tiling/TilingLayout.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "core/process/ProcessEnvironment.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include "support/TestHarness.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/filesystem/TemporaryDirectory.hpp"
#include <chrono>
#include <cmath>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <thread>
#include <string>
#include <vector>

namespace LunaDash {
namespace {
void writeLegacySettings(const std::filesystem::path &path,
                         const std::string &contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
}
} // namespace

class WindowLayoutTests final {
  TemporaryDirectory home_;
  const Rect area{10, 40, 1440, 900};
  // Window geometry by id; missing windows read as an empty rectangle.
  struct GeometryMap : std::map<LayoutWindowId, Rect> {
    using std::map<LayoutWindowId, Rect>::operator[];
    Rect operator[](LayoutWindowId window) const { return value(window); }
    Rect value(LayoutWindowId window) const {
      const auto found = find(window);
      return found == end() ? Rect() : found->second;
    }
  };
  static GeometryMap
  geometries(const std::vector<WindowPlacement> &placements) {
    GeometryMap result;
    for (const auto &slot : placements)
      if (!slot.minimized)
        result[slot.window] = slot.geometry;
    return result;
  }
  static void verifyNoOverlap(const std::vector<WindowPlacement> &placements) {
    for (const auto &a : placements) {
      if (a.minimized)
        continue;
      VERIFY(a.geometry.isValid());
      for (const auto &b : placements)
        if (!b.minimized && a.window != b.window)
          VERIFY(!a.geometry.intersects(b.geometry));
    }
  }
  static Json areaJson(Rect area) {
    return {{"x", area.x()},
            {"y", area.y()},
            {"width", area.width()},
            {"height", area.height()}};
  }
  static bool focusDirection(WindowLayout &layout, LayoutWorkspaceId workspace,
                             int dx, int dy) {
    return layout.performAction("focus-direction",
                                {{"workspace", static_cast<std::int64_t>(workspace)},
                                 {"dx", dx},
                                 {"dy", dy}});
  }
  static bool groupWith(WindowLayout &layout, LayoutWindowId window,
                        LayoutWindowId target) {
    return layout.performAction("group-with",
                                {{"window", static_cast<std::int64_t>(window)},
                                 {"target", static_cast<std::int64_t>(target)}});
  }
  static bool expel(WindowLayout &layout, LayoutWindowId window) {
    return layout.performAction("expel",
                                {{"window", static_cast<std::int64_t>(window)}});
  }
  static bool swapWindows(WindowLayout &layout, LayoutWindowId window,
                          LayoutWindowId target) {
    return layout.performAction("swap",
                                {{"window", static_cast<std::int64_t>(window)},
                                 {"target", static_cast<std::int64_t>(target)}});
  }
  static bool insertBeside(WindowLayout &layout, LayoutWindowId window,
                           LayoutWindowId target, bool after) {
    return layout.performAction("insert-beside",
                                {{"window", static_cast<std::int64_t>(window)},
                                 {"target", static_cast<std::int64_t>(target)},
                                 {"after", after}});
  }
  static bool resizeWidth(WindowLayout &layout, LayoutWindowId window,
                          int width) {
    return layout.performAction(
        "resize-width",
        {{"window", static_cast<std::int64_t>(window)}, {"width", width}});
  }
  static bool resizeHeight(WindowLayout &layout, LayoutWindowId window,
                           int height) {
    return layout.performAction(
        "resize-height",
        {{"window", static_cast<std::int64_t>(window)}, {"height", height}});
  }
  static bool moveBy(WindowLayout &layout, LayoutWindowId window, Point delta,
                     Rect area) {
    return layout.performAction("move-by",
                                {{"window", static_cast<std::int64_t>(window)},
                                 {"dx", delta.x()},
                                 {"dy", delta.y()},
                                 {"area", areaJson(area)}});
  }
public:
  void initTestCase() {
    VERIFY(home_.isValid());
    for (const auto *name :
         {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"})
      ::setenv(name, (home_.path() / name).c_str(), 1);
  }
  void layoutTemplateContract() {
    const auto templates = windowTemplates();
    COMPARE(templates.size(), 3);
    COMPARE(templates[0].key, std::string("tiling"));
    COMPARE(templates[2].key, std::string("scrolling"));
    VERIFY(!templates[2].allowOverlap && !templates[2].activationTogglesMaximize);
    VERIFY(templates[2].layoutSettingsSchema.contains("columnWidth"));
    VERIFY(!templates[0].clientMoveResize);
    VERIFY(templates[0].activationTogglesMaximize);
    COMPARE(templates[1].key, std::string("stacking"));
    VERIFY(templates[1].clientMoveResize);
    VERIFY(!templates[1].activationTogglesMaximize);
    VERIFY(createWindowLayout(windowTemplateForKey("stacking")));
    auto layout = createWindowLayout(windowTemplateForKey("tiling"));
    VERIFY(layout);
    VERIFY(!templates[0].allowOverlap);
    VERIFY(templates[1].allowOverlap);
    VERIFY(templates[0].layoutSettingsSchema.contains("gap"));
    VERIFY(templates[0].layoutSettingsSchema.contains("defaultWidth"));
    COMPARE(
        jsonString(templates[0].layoutSettingsDefaults, "splitTarget"),
        std::string("focused"));
    COMPARE(
        jsonString(templates[0].layoutSettingsDefaults, "firstWindowSide"),
        std::string("right"));
    VERIFY(templates[0].layoutActions.contains("group-direction"));
    VERIFY(templates[0].layoutActions.contains("insert-beside"));
    VERIFY(!templates[1].layoutActions.contains("group-direction"));
    std::string settingError;
    VERIFY(validateWindowLayoutSettings(
        templates[0], {{"gap", 8}, {"defaultWidth", 1000}}, &settingError));
    VERIFY(!validateWindowLayoutSettings(templates[0], {{"gap", 999}},
                                          &settingError));
    VERIFY(validateWindowLayoutSettings(
        templates[0], {{"splitTarget", "largest"}, {"firstWindowSide", "left"}},
        &settingError));
    VERIFY(!validateWindowLayoutSettings(
        templates[0], {{"splitTarget", "unknown"}}, &settingError));
    VERIFY(!validateWindowLayoutSettings(
        templates[0], {{"firstWindowSide", "top"}}, &settingError));
    layout->configure(templates[0].layoutSettingsDefaults);
    VERIFY(!performWindowLayoutAction(*layout, templates[0], "missing"));
    VERIFY(layout->insert(0, 1));
    VERIFY(layout->insert(0, 2));
    verifyNoOverlap(layout->layout(0, area));
    VERIFY(layout->setMaximized(2, true));
    COMPARE(geometries(layout->presentation(0, area))[2], area);
    VERIFY(layout->setMaximized(2, false));
    verifyNoOverlap(layout->presentation(0, area));
  }
  void stackingRetainsIndependentGeometry() {
    auto layout = createWindowLayout(windowTemplateForKey("stacking"));
    VERIFY(layout->insert(0, 1, Size(800, 500)));
    VERIFY(layout->insert(0, 2, Size(800, 500)));
    auto initial = geometries(layout->layout(0, area));
    VERIFY(initial[1].intersects(initial[2]));
    VERIFY(moveBy(*layout, 1, Point(-100, 80), area));
    VERIFY(resizeWidth(*layout, 1, 650));
    VERIFY(resizeHeight(*layout, 1, 450));
    const auto resized = geometries(layout->layout(0, area));
    COMPARE(resized[1].size(), Size(650, 450));
    COMPARE(resized[2], initial[2]);
    VERIFY(layout->setMaximized(1, true));
    const auto maximized = layout->presentation(0, area);
    COMPARE(geometries(maximized)[1], area);
    for (const auto &window : maximized)
      VERIFY(!window.hiddenByMaximize);
    VERIFY(layout->setMaximized(1, false));
    COMPARE(geometries(layout->presentation(0, area)), resized);
    VERIFY(layout->focus(1));
    COMPARE(layout->snapshot(0).columns.back().window, LayoutWindowId(1));
    VERIFY(layout->setMinimized(1, true));
    COMPARE(layout->snapshot(0).focusedWindow, LayoutWindowId(2));
    VERIFY(layout->setMinimized(1, false));
    COMPARE(geometries(layout->layout(0, area)), resized);
    VERIFY(layout->moveToWorkspace(1, 1));
    COMPARE(layout->snapshot(0).columns.size(), 1);
    COMPARE(layout->snapshot(1).columns.size(), 1);
    VERIFY(moveBy(*layout, 1, Point(100000, -100000), area));
    for (const auto &window : layout->layout(1, Rect(0, 0, 300, 200)))
      VERIFY(Rect(0, 0, 300, 200).contains(window.geometry));
    VERIFY(layout->remove(1));
    COMPARE(layout->snapshot(1).focusedWindow, LayoutWindowId(0));
  }
  void fixedSplitsStayOnScreen() {
    TilingLayout layout;
    layout.configure({{"defaultWidth", 960},
                      {"gap", 6},
                      {"splitTarget", "largest"},
                      {"firstWindowSide", "left"}});
    VERIFY(layout.insert(0, 1, Size(900, 600)));
    COMPARE(layout.layout(0, area).front().geometry.size(), Size(900, 600));
    VERIFY(layout.insert(0, 2));
    auto two = geometries(layout.layout(0, area));
    COMPARE(two[1].height(), area.height());
    COMPARE(two[2].height(), area.height());
    COMPARE(two[1].right() + 7, two[2].left());
    VERIFY(layout.insert(0, 3));
    auto three = geometries(layout.layout(0, area));
    COMPARE(three[1], two[1]);
    COMPARE(three[2].left(), two[2].left());
    COMPARE(three[3].left(), two[2].left());
    COMPARE(three[2].bottom() + 7, three[3].top());
    VERIFY(layout.focus(1));
    const auto unchanged = geometries(layout.layout(0, area));
    COMPARE(unchanged, three); // Focusing never scrolls or rearranges slots.
    VERIFY(focusDirection(layout, 0, 1, 0));
    VERIFY(focusDirection(layout, 0, 0, 1));
    COMPARE(layout.snapshot(0).focusedWindow, LayoutWindowId(3));
    VERIFY(resizeWidth(layout, 3, 900));
    VERIFY(resizeHeight(layout, 3, 600));
    auto resized = layout.layout(0, area);
    verifyNoOverlap(resized);
    for (const auto &slot : resized)
      VERIFY(area.contains(slot.geometry));
    const auto beforeMinimize = geometries(resized);
    VERIFY(layout.setMinimized(2, true));
    verifyNoOverlap(layout.layout(0, area));
    VERIFY(layout.setMinimized(2, false));
    COMPARE(geometries(layout.layout(0, area)), beforeMinimize);
    VERIFY(layout.remove(3));
    COMPARE(geometries(layout.layout(0, area))[2].height(), area.height());
  }
  void focusedSplitsPreserveMainPane() {
    TilingLayout layout;
    layout.configure(
        {{"gap", 8}, {"minimumTileWidth", 1}, {"minimumTileHeight", 1}});
    const Rect bounds(16, 48, 1608, 1000);
    VERIFY(layout.insert(0, 1));
    layout.layout(0, bounds);
    VERIFY(layout.insert(0, 2));
    const auto pair = geometries(layout.layout(0, bounds));
    COMPARE(pair[1], Rect(824, 48, 800, 1000));
    COMPARE(pair[2], Rect(16, 48, 800, 1000));

    VERIFY(layout.insert(0, 3));
    const auto three = geometries(layout.layout(0, bounds));
    COMPARE(three[1], pair[1]);
    COMPARE(three[3], Rect(16, 552, 800, 496));
    VERIFY(layout.focus(2));
    COMPARE(geometries(layout.layout(0, bounds)), three);
    VERIFY(layout.insert(0, 4));
    layout.layout(0, bounds);
    VERIFY(layout.insert(0, 5));
    const auto five = geometries(layout.layout(0, bounds));
    VERIFY(layout.insert(0, 6));
    const auto placements = layout.layout(0, bounds);
    const auto six = geometries(placements);
    COMPARE(six.size(), 6);
    COMPARE(six[1], pair[1]);
    COMPARE(six[2], Rect(16, 48, 396, 496));
    COMPARE(six[3], three[3]);
    COMPARE(six[4], Rect(420, 48, 396, 244));
    COMPARE(six[5], Rect(420, 300, 194, 244));
    COMPARE(six[6], Rect(622, 300, 194, 244));
    verifyNoOverlap(placements);
    for (const auto &slot : placements)
      VERIFY(bounds.contains(slot.geometry));

    VERIFY(layout.setMaximized(6, true));
    COMPARE(geometries(layout.presentation(0, bounds))[6], bounds);
    VERIFY(layout.setMaximized(6, false));
    COMPARE(geometries(layout.presentation(0, bounds)), six);
    VERIFY(layout.setMinimized(4, true));
    verifyNoOverlap(layout.layout(0, bounds));
    VERIFY(layout.setMinimized(4, false));
    COMPARE(geometries(layout.layout(0, bounds)), six);
    VERIFY(layout.remove(6));
    COMPARE(geometries(layout.layout(0, bounds)), five);

    // A live policy edit only affects later insertions, never saved slots.
    layout.configure({{"splitTarget", "largest"}, {"firstWindowSide", "left"}});
    COMPARE(geometries(layout.layout(0, bounds)), five);
    VERIFY(layout.insert(0, 7));
    const auto changed = geometries(layout.layout(0, bounds));
    COMPARE(changed[2], five[2]);
    COMPARE(changed[3], five[3]);
    COMPARE(changed[4], five[4]);
    COMPARE(changed[5], five[5]);
    COMPARE(changed[1], Rect(824, 48, 800, 496));
    COMPARE(changed[7], Rect(824, 552, 800, 496));
  }
  void focusedSplitUsesDestinationFocus() {
    TilingLayout layout;
    layout.configure({{"gap", 8}, {"firstWindowSide", "left"}});
    const Rect bounds(16, 48, 1608, 1000);
    VERIFY(layout.insert(0, 1));
    layout.layout(0, bounds);
    VERIFY(layout.insert(0, 2));
    const auto pair = geometries(layout.layout(0, bounds));
    COMPARE(pair[1], Rect(16, 48, 800, 1000));
    COMPARE(pair[2], Rect(824, 48, 800, 1000));
    VERIFY(layout.focus(1));
    VERIFY(layout.insert(1, 10));
    VERIFY(layout.moveToWorkspace(10, 0));
    const auto moved = geometries(layout.layout(0, bounds));
    COMPARE(moved[2], pair[2]);
    COMPARE(moved[1], Rect(16, 48, 800, 496));
    COMPARE(moved[10], Rect(16, 552, 800, 496));
    COMPARE(layout.snapshot(1).columns.size(), 0);
    COMPARE(layout.snapshot(0).focusedWindow, LayoutWindowId(10));

    for (const auto id : {1, 2, 10})
      VERIFY(layout.setMinimized(id, true));
    VERIFY(layout.insert(0, 11));
    for (const auto id : {1, 2, 10})
      VERIFY(layout.setMinimized(id, false));
    const auto restored = layout.layout(0, bounds);
    COMPARE(restored.size(), 4);
    verifyNoOverlap(restored);
    for (const auto &slot : restored)
      VERIFY(bounds.contains(slot.geometry));
  }
  void focusedSplitsPreferReadableTiles() {
    TilingLayout layout;
    layout.configure({{"gap", 12}});
    const Rect bounds(12, 72, 1896, 996);
    for (int id = 1; id <= 6; ++id) {
      if (id == 4)
        VERIFY(layout.focus(2));
      VERIFY(layout.insert(0, id));
      const auto placements = layout.layout(0, bounds);
      verifyNoOverlap(placements);
      for (const auto &slot : placements) {
        VERIFY(bounds.contains(slot.geometry));
        VERIFY(slot.geometry.width() >= 320);
        VERIFY(slot.geometry.height() >= 300);
      }
    }
    const auto six = geometries(layout.layout(0, bounds));
    // The fifth window uses the large right tile instead of halving the
    // upper-left branch into two 240 px high windows. The sixth then fits
    // beside it without further reducing the content height.
    COMPARE(six[1], Rect(966, 72, 942, 492));
    COMPARE(six[6], Rect(1443, 576, 465, 492));
    COMPARE(six[5], Rect(966, 576, 465, 492));

    // On a crowded screen the preference remains a soft bound: every
    // application still has a non-overlapping, on-screen slot.
    for (int id = 7; id <= 20; ++id) {
      VERIFY(layout.insert(0, id));
      const auto placements = layout.layout(0, bounds);
      verifyNoOverlap(placements);
      for (const auto &slot : placements)
        VERIFY(bounds.contains(slot.geometry));
    }
  }
  void manyWindowsAndOutputChanges() {
    for (const Rect bounds :
         {area, Rect(-800, 20, 800, 1200), Rect(0, 0, 320, 240)}) {
      TilingLayout layout;
      layout.configure({{"defaultWidth", 960}, {"gap", 6}});
      for (int id = 1; id <= 64; ++id) {
        VERIFY(layout.insert(0, id));
        const auto placements = layout.layout(0, bounds);
        COMPARE(placements.size(), id);
        verifyNoOverlap(placements);
        for (const auto &slot : placements)
          VERIFY(bounds.contains(slot.geometry));
      }
      for (const Rect resized : {Rect(0, 0, 500, 300), area}) {
        const auto placements = layout.layout(0, resized);
        COMPARE(placements.size(), 64);
        verifyNoOverlap(placements);
        for (const auto &slot : placements)
          VERIFY(resized.contains(slot.geometry));
      }
    }
  }
  void mixedLayoutEditsRemainBounded() {
    TilingLayout layout;
    layout.configure({{"defaultWidth", 960}, {"gap", 6}});
    for (int id = 1; id <= 16; ++id)
      layout.insert(0, id);
    std::uint32_t random = 12345;
    for (int step = 0; step < 400; ++step) {
      random = random * 1664525U + 1013904223U;
      const int id = 1 + (random >> 8) % 16;
      const int target = 1 + (random >> 16) % 16;
      switch (step % 8) {
      case 0:
        groupWith(layout, id, target);
        break;
      case 1:
        expel(layout, id);
        break;
      case 2:
        swapWindows(layout, id, target);
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
        resizeWidth(layout, id, 120 + target * 60);
        break;
      case 7:
        resizeHeight(layout, id, 80 + target * 40);
        break;
      }
      int total = 0;
      for (int workspace = 0; workspace < 2; ++workspace) {
        const auto placements = layout.layout(workspace, area);
        total += placements.size();
        verifyNoOverlap(placements);
        for (const auto &slot : placements)
          if (!slot.minimized)
            VERIFY(area.contains(slot.geometry));
      }
      COMPARE(total, 16);
    }
  }
  void singleWindowProportionsAndBounds() {
    TilingLayout layout;
    layout.insert(0, 1, Size(2400, 1600));
    const auto fitted = layout.layout(0, area).front().geometry;
    VERIFY(area.contains(fitted));
    COMPARE(fitted.width() * 2, fitted.height() * 3);
    VERIFY(moveBy(layout, 1, Point(10000, -10000), area));
    VERIFY(area.contains(layout.layout(0, area).front().geometry));
    VERIFY(resizeWidth(layout, 1, 10000));
    VERIFY(resizeHeight(layout, 1, 10000));
    COMPARE(layout.layout(0, area).front().geometry, area);
  }
  void eightRowsAndOverflow() {
    TilingLayout layout;
    // Keep the dense reference tree for this grouping-capacity regression.
    // Preferred split sizes have separate insertion coverage above.
    layout.configure({{"minimumTileWidth", 1}, {"minimumTileHeight", 1}});
    for (int i = 1; i <= 9; ++i)
      VERIFY(layout.insert(0, i));
    auto placements = layout.layout(0, area);
    for (int i = 0; i < 9; ++i)
      COMPARE(jsonInt(placements[i].metadata, "group"), i);
    for (int i = 2; i <= 8; ++i)
      VERIFY(groupWith(layout, i, i - 1));
    placements = layout.layout(0, area);
    COMPARE(placements.size(), 9);
    verifyNoOverlap(placements);
    for (int i = 0; i < 8; ++i) {
      COMPARE(jsonInt(placements[i].metadata, "group"), 0);
      COMPARE(jsonInt(placements[i].metadata, "row"), i);
    }
    COMPARE(jsonInt(placements[8].metadata, "group"), 1);
    COMPARE(placements[7].geometry.bottom(), area.bottom());
    VERIFY(!groupWith(layout, 9, 1));
    VERIFY(layout.setMinimized(4, true));
    VERIFY(!groupWith(layout, 9, 1)); // Minimized clients retain their slot.
    verifyNoOverlap(layout.layout(0, area));
    VERIFY(layout.setMinimized(4, false));
    VERIFY(layout.remove(4));
    VERIFY(groupWith(layout, 9, 1));
    placements = layout.layout(0, area);
    COMPARE(placements.size(), 8);
    for (const auto &slot : placements)
      COMPARE(jsonInt(slot.metadata, "group"), 0);
    verifyNoOverlap(placements);
  }
  void swapsPreserveSlots() {
    TilingLayout layout;
    for (int i = 1; i <= 3; ++i)
      layout.insert(0, i);
    groupWith(layout, 2, 1);
    groupWith(layout, 3, 2);
    layout.layout(0, area);
    VERIFY(resizeHeight(layout, 2, 500));
    auto before = geometries(layout.layout(0, area));
    VERIFY(swapWindows(layout, 1, 2));
    auto after = geometries(layout.layout(0, area));
    COMPARE(after[1], before[2]);
    COMPARE(after[2], before[1]);
    COMPARE(after[3], before[3]);
    VERIFY(expel(layout, 3));
    VERIFY(insertBeside(layout, 3, 1, false));
    auto placements = layout.layout(0, area);
    COMPARE(placements[1].window, LayoutWindowId(3));
    COMPARE(placements[2].window, LayoutWindowId(1));
    verifyNoOverlap(placements);
  }
  void resizingAndSingleMovement() {
    TilingLayout layout;
    layout.insert(0, 1);
    const auto before = layout.layout(0, area).front().geometry;
    VERIFY(moveBy(layout, 1, Point(60, 30), area));
    const auto moved = layout.layout(0, area).front().geometry;
    COMPARE(moved.size(), before.size());
    COMPARE(moved.topLeft(), before.topLeft() + Point(60, 30));
    VERIFY(resizeHeight(layout, 1, 400));
    COMPARE(layout.layout(0, area).front().geometry.height(), 400);
    COMPARE(layout.layout(0, area).front().geometry.topLeft(),
             moved.topLeft());
    VERIFY(resizeHeight(layout, 1, 700));
    COMPARE(layout.layout(0, area).front().geometry.height(), 700);
    layout.insert(0, 2);
    layout.insert(0, 3);
    groupWith(layout, 2, 1);
    groupWith(layout, 3, 2);
    verifyNoOverlap(layout.layout(0, area));
    VERIFY(!moveBy(layout, 1, Point(10, 10), area));
    for (const int height : {600, 50, 10000, 300}) {
      VERIFY(resizeHeight(layout, 2, height));
      auto placements = layout.layout(0, area);
      verifyNoOverlap(placements);
      COMPARE(placements[1].window, LayoutWindowId(2));
      COMPARE(placements.back().geometry.bottom(), area.bottom());
      if (height == 600 || height == 300)
        COMPARE(placements[1].geometry.height(), height);
    }
    VERIFY(layout.moveToWorkspace(2, 1));
    COMPARE(layout.layout(0, area).size(), 2);
    COMPARE(layout.layout(1, area).size(), 1);
  }
  void workspaceMaximizePreservesTiling() {
    TilingLayout layout;
    layout.configure({{"defaultWidth", 450}, {"gap", 6}});
    for (int id = 1; id <= 4; ++id)
      VERIFY(layout.insert(0, id));
    VERIFY(groupWith(layout, 2, 1));
    VERIFY(layout.insert(1, 10, Size(500, 400)));
    layout.layout(0, area);
    VERIFY(resizeHeight(layout, 2, 300));
    VERIFY(layout.focus(2));
    const auto tiled = geometries(layout.layout(0, area));
    const auto otherWorkspace = geometries(layout.layout(1, area));
    VERIFY(layout.setMaximized(2, true));
    const auto expanded = layout.presentation(0, area);
    COMPARE(expanded.size(), 4);
    for (const auto &slot : expanded) {
      COMPARE(slot.hiddenByMaximize, slot.window != 2);
      VERIFY(!slot.minimized);
      if (slot.window == 2)
        COMPARE(slot.geometry, area);
    }
    COMPARE(geometries(layout.layout(0, area)), tiled);
    COMPARE(geometries(layout.presentation(1, area)), otherWorkspace);
    VERIFY(layout.setMaximized(2, false));
    const auto restored = layout.presentation(0, area);
    COMPARE(geometries(restored), tiled);
    for (const auto &slot : restored)
      VERIFY(!slot.hiddenByMaximize);
    verifyNoOverlap(restored);
    VERIFY(layout.setMaximized(3, true));
    VERIFY(layout.setMaximized(1, true));
    COMPARE(layout.snapshot(0).maximizedWindow, LayoutWindowId(1));
    for (const auto &slot : layout.presentation(0, area))
      COMPARE(slot.hiddenByMaximize, slot.window != 1);
    VERIFY(layout.setMinimized(4, true));
    VERIFY(layout.setMinimized(1, true));
    COMPARE(layout.snapshot(0).maximizedWindow, LayoutWindowId(0));
    VERIFY(!layout.setMaximized(1, true));
    VERIFY(layout.setMinimized(1, false));
    VERIFY(layout.setMaximized(2, true));
    VERIFY(layout.remove(2));
    COMPARE(layout.snapshot(0).maximizedWindow, LayoutWindowId(0));
    VERIFY(layout.setMaximized(3, true));
    VERIFY(layout.moveToWorkspace(3, 1));
    COMPARE(layout.snapshot(0).maximizedWindow, LayoutWindowId(0));
    for (const auto &slot : layout.presentation(0, area)) {
      VERIFY(!slot.hiddenByMaximize);
      if (slot.window == 4)
        VERIFY(slot.minimized);
    }
  }
  void freeformSwapExchangesWindowSlots() {
    const auto &stacking = windowTemplateForKey("stacking");
    auto layout = createWindowLayout(stacking);
    VERIFY(layout);
    VERIFY(layout->insert(0, 1, Size(420, 320)));
    VERIFY(layout->insert(0, 2, Size(420, 320)));
    layout->presentation(0, area);
    VERIFY(performWindowLayoutAction(
        *layout, stacking, "move-by",
        {{"window", 1}, {"dx", -180}, {"dy", 0}, {"area", areaJson(area)}}));
    VERIFY(performWindowLayoutAction(
        *layout, stacking, "move-by",
        {{"window", 2}, {"dx", 180}, {"dy", 0}, {"area", areaJson(area)}}));
    const auto before = geometries(layout->presentation(0, area));
    VERIFY(before.value(1) != before.value(2));
    VERIFY(performWindowLayoutAction(*layout, stacking, "swap",
                                      {{"window", 1}, {"target", 2}}));
    const auto after = geometries(layout->presentation(0, area));
    COMPARE(after.value(1), before.value(2));
    COMPARE(after.value(2), before.value(1));
  }

  void scrollingColumnsKeepTheirWidth() {
    const auto &scrolling = windowTemplateForKey("scrolling");
    COMPARE(scrolling.key, std::string("scrolling"));
    auto layout = createWindowLayout(scrolling);
    VERIFY(layout);
    layout->configure({{"gap", 10}, {"columnWidth", 50}, {"centerFocusedColumn", "never"}});
    // One column is centred at its own width instead of filling the screen.
    VERIFY(layout->insert(0, 1));
    auto placed = geometries(layout->presentation(0, area));
    COMPARE(placed.value(1).width(), (area.width() - 20) / 2);
    COMPARE(placed.value(1).center().x(), area.center().x());
    COMPARE(placed.value(1).height(), area.height() - 20);
    // Three half-width columns overflow: the focused (newest) one stays visible.
    VERIFY(layout->insert(0, 2));
    VERIFY(layout->insert(0, 3));
    placed = geometries(layout->presentation(0, area));
    COMPARE(placed.value(1).width(), placed.value(3).width());
    VERIFY(placed.value(3).right() <= area.right());
    VERIFY(placed.value(1).left() < area.left());
    // Focusing left scrolls back; widths never change.
    VERIFY(performWindowLayoutAction(*layout, scrolling, "focus-direction",
                                     {{"workspace", 0}, {"dx", -1}, {"dy", 0}}));
    VERIFY(performWindowLayoutAction(*layout, scrolling, "focus-direction",
                                     {{"workspace", 0}, {"dx", -1}, {"dy", 0}}));
    placed = geometries(layout->presentation(0, area));
    VERIFY(placed.value(1).left() >= area.left());
    COMPARE(layout->snapshot(0).focusedWindow, LayoutWindowId(1));
    // Consuming into a column stacks windows; expelling restores a column.
    VERIFY(performWindowLayoutAction(*layout, scrolling, "group-direction",
                                     {{"window", 2}, {"workspace", 0}, {"dx", -1}, {"dy", 0}}));
    placed = geometries(layout->presentation(0, area));
    COMPARE(placed.value(1).x(), placed.value(2).x());
    VERIFY(placed.value(2).top() > placed.value(1).bottom());
    VERIFY(performWindowLayoutAction(*layout, scrolling, "expel", {{"window", 2}}));
    placed = geometries(layout->presentation(0, area));
    VERIFY(placed.value(2).x() > placed.value(1).x());
    // Resizing changes only that column; maximize overlays the work area.
    VERIFY(performWindowLayoutAction(*layout, scrolling, "resize-width",
                                     {{"window", 3}, {"width", 900}}));
    placed = geometries(layout->presentation(0, area));
    COMPARE(placed.value(3).width(),
            static_cast<int>(std::lround(900.0 / area.width() * (area.width() - 20))));
    VERIFY(layout->setMaximized(3, true));
    placed = geometries(layout->presentation(0, area));
    COMPARE(placed.value(3), area);
    VERIFY(layout->remove(1));
    VERIFY(layout->remove(2));
    VERIFY(layout->remove(3));
    VERIFY(layout->presentation(0, area).empty());
  }

  void geometryBounds() {
    LuDashRectangle halves[2];
    for (const int vertical : {0, 1}) {
      VERIFY(ludash_split_rectangle({-10, 20, 31, 29}, vertical, 500000, 6, 1,
                                     1, halves));
      const auto a =
          Rect(halves[0].x, halves[0].y, halves[0].width, halves[0].height);
      const auto b =
          Rect(halves[1].x, halves[1].y, halves[1].width, halves[1].height);
      VERIFY(!a.intersects(b));
      VERIFY(Rect(-10, 20, 31, 29).contains(a));
      VERIFY(Rect(-10, 20, 31, 29).contains(b));
    }
    VERIFY(ludash_split_rectangle({0, 0, 2, 8}, 1, 0, INT_MAX, 1, 1, halves));
    COMPARE(halves[0].width, 1);
    COMPARE(halves[1].x, 1);
    VERIFY(!ludash_split_rectangle({0, 0, 1, 8}, 1, 500000, 0, 1, 1, halves));
    VERIFY(!ludash_split_rectangle({INT_MAX, 0, 10, 8}, 1, 500000, 0, 1, 1,
                                    halves));
    LuDashRectangle output[9];
    for (int count = 1; count <= 8; ++count) {
      const LuDashRectangle small{0, 0, 120, count};
      COMPARE(ludash_layout_column_windows(small, count, 100, output, 9),
               size_t(count));
      for (int i = 0; i < count; ++i) {
        COMPARE(output[i].height, 1);
        COMPARE(output[i].y, i);
      }
    }
    COMPARE(ludash_layout_column_windows({0, 0, 100, 100}, 9, 4, output, 9),
             size_t(0));
    int invalid[] = {INT_MAX, 0};
    COMPARE(ludash_layout_weighted_column_windows({0, 0, 100, 100}, invalid, 2,
                                                   4, output, 9),
             size_t(0));
  }
  void windowSelectionScopeAndLifetime() {
    Json clients = Json::array();
    for (int i = 1; i <= 6; ++i)
      clients.push_back(Json::object({{"id", i},
                                 {"mapped", i != 5},
                                 {"workspace", i == 4 ? 1 : 0},
                                 {"utility", i == 6},
                                 {"desktop", i == 3},
                                 {"minimized", i == 2}}));
    const auto windows = WindowSwitcher::windowsForWorkspace(clients, 0);
    COMPARE(windows.size(),
             2); // Includes minimized peers, no other workspace.
    WindowSwitcher switcher;
    ProcessEnvironment environment;
    VERIFY(switcher.begin(windows, 1, 1, environment,
                           WindowSwitcher::Scope::Windows));
    COMPARE(jsonString(switcher.snapshot(), "scope"), std::string("windows"));
    VERIFY(jsonArray(switcher.snapshot(), "workspaces").empty());
    COMPARE(jsonArray(switcher.snapshot(), "windows").size(), 2);
    COMPARE(switcher.finish(true), 2);
    VERIFY(switcher.begin(windows, 1, -1, environment,
                           WindowSwitcher::Scope::Windows));
    switcher.remove(1);
    COMPARE(jsonInt(switcher.snapshot(), "index"), 0);
    COMPARE(switcher.finish(true), 2);
    switcher.begin(windows, 1, 1, environment, WindowSwitcher::Scope::Windows);
    switcher.remove(2);
    switcher.remove(1);
    VERIFY(!switcher.active());
    COMPARE(switcher.finish(true), 0);
    switcher.dismissPopups();
    COMPARE(jsonInt(switcher.snapshot(), "dismissSerial"), 1);
    switcher.dismissPopups();
    COMPARE(jsonInt(switcher.snapshot(), "dismissSerial"), 2);
    VERIFY(
        !switcher.begin({}, 0, 1, environment, WindowSwitcher::Scope::Windows));
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
  }
  void selectionLifecycle() {
    TemporaryDirectory runtime;
    WindowSwitcher switcher;
    const std::string channel = runtime.filePath("interaction.json");
    switcher.setChannelPath(channel);
    switcher.setLauncherState(7, true);
    COMPARE(jsonInt(switcher.snapshot(), "launcherSerial"), 7);
    VERIFY(jsonBool(switcher.snapshot(), "launcherOpen"));
    switcher.setLauncherState(8, false);
    COMPARE(jsonInt(switcher.snapshot(), "launcherSerial"), 8);
    VERIFY(!jsonBool(switcher.snapshot(), "launcherOpen"));
    ProcessEnvironment environment{
        {"XDG_RUNTIME_DIR", runtime.path().string()},
        {"WAYLAND_DISPLAY", runtime.filePath("no-display")},
    };
    Json workspaces = Json::array();
    for (int i = 1; i <= 10; ++i)
      workspaces.push_back(Json::object({
          {"id", i}, {"windows", Json{Json{{"id", i * 10}}}}}));
    VERIFY(switcher.begin(workspaces, 1, 1, environment));
    const int serial = switcher.serial();
    VERIFY(switcher.begin(workspaces, 1, 1, environment));
    COMPARE(switcher.serial(), serial);
    switcher.step(-1);
    COMPARE(switcher.finish(true), 2);
    VERIFY(switcher.begin(workspaces, 1, -1, environment));
    COMPARE(switcher.finish(true), 10);
    switcher.begin(workspaces, 1, 5, environment);
    COMPARE(switcher.finish(true), 6);
    switcher.begin(workspaces, 1, 1, environment);
    switcher.remove(20);
    COMPARE(jsonArray(switcher.snapshot(), "workspaces").size(), 10);
    VERIFY(jsonArray(jsonArray(switcher.snapshot(), "workspaces")[1], "windows")
               .empty());
    VERIFY(switcher.select(10));
    VERIFY(!switcher.select(11));
    COMPARE(switcher.finish(false), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const auto contents = readFile(channel);
    VERIFY(contents.has_value());
    VERIFY(!jsonBool(parseJson(*contents), "active"));
    VERIFY((std::filesystem::status(channel).permissions() &
            std::filesystem::perms::others_read) == std::filesystem::perms::none);
    // Reap only our failed captures; no compositor is started.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  void externalFileManagerDefaults() {
    TemporaryDirectory config;
    ::setenv("XDG_CONFIG_HOME", config.path().c_str(), 1);
    VERIFY(defaultApplicationCommand("files", nullptr) ==
            (std::vector<std::string>{"dolphin", "--new-window"}));
    // Keep a user's explicit argv intact, including spaces and shell
    // characters.
    const std::vector<std::string> custom{"/usr/bin/printf", "--profile",
                             "Work $HOME; notes"};
    writeLegacySettings(config.path() / "LunaDash/LunaDash.conf",
                        "[defaultApps]\n"
                        "files=/usr/bin/printf, --profile, \"Work $HOME; notes\"\n");
    auto configured = defaultApplicationCommand("files", nullptr);
    COMPARE(configured.size(), custom.size());
    for (std::size_t index = 0; index < custom.size(); ++index)
      COMPARE(configured[index], custom[index]);
    std::string error;
    for (const auto &launcher :
         {"ludash-desktop", "lunadash-desktop", "ludashctl", "lunadashctl"})
      VERIFY(!setDefaultApplications(
          {{"files", nlohmann::json::array({launcher, "--app", "files"})}},
          &error));
    VERIFY(
        setDefaultApplications({{"files", nlohmann::json::array()}}, &error));
    VERIFY(defaultApplicationCommand("files", nullptr) ==
            (std::vector<std::string>{"dolphin", "--new-window"}));
  }
  void terminalAndShortcutMigration() {
    TemporaryDirectory config;
    struct ConfigHomeRestore {
      const char *previous = std::getenv("XDG_CONFIG_HOME");
      std::string value = previous ? previous : "";
      ~ConfigHomeRestore() {
        if (!previous)
          ::unsetenv("XDG_CONFIG_HOME");
        else
          ::setenv("XDG_CONFIG_HOME", value.c_str(), 1);
      }
    } restoreConfigHome;
    const auto useNativeConfig = [&config](const std::string &name) {
      ::setenv("XDG_CONFIG_HOME", (config.path() / name).c_str(), 1);
    };
    useNativeConfig("defaults");
    VERIFY(defaultApplicationCommand("terminal", nullptr) ==
            (std::vector<std::string>{"kitty"}));
    ShortcutSettings settings;
    VERIFY(settings.actionFor(XKB_KEY_t, ShortcutMeta) == "launchTerminal");
    VERIFY(settings.actionFor(XKB_KEY_0, ShortcutMeta) == "workspace10");
    VERIFY(settings.actionFor(XKB_KEY_Return, ShortcutMeta).empty());
    std::string error;
    VERIFY(!settings.apply({{"closeWindow", "Alt+Tab"}}, &error));
    VERIFY(!settings.apply({{"closeWindow", "Meta+Tab"}}, &error));
    VERIFY(!settings.apply({{"closeWindow", "Meta+Shift+Tab"}}, &error));
    VERIFY(!settings.apply({{"closeWindow", "Alt+Shift+Tab"}}, &error));

    useNativeConfig("legacy");
    ShortcutSettings migrated(Json::object({{"launchTerminal", "Meta+Return"},
                                            {"closeWindow", "Meta+T"},
                                            {"toggleFloating", "Meta+Space"}}));
    VERIFY(migrated.actionFor(XKB_KEY_t, ShortcutMeta) == "closeWindow");
    VERIFY(migrated.actionFor(XKB_KEY_Return, ShortcutMeta) ==
            "launchTerminal");
    VERIFY(!loadShortcutBindings().contains("toggleFloating"));
    ShortcutSettings retained(Json::object({{"closeWindow", "Meta+Q"}}));
    VERIFY(retained.actionFor(XKB_KEY_t, ShortcutMeta) == "closeWindow");

    // Removed shortcut IDs from older releases must not stay in the persistent
    // settings map or reappear in Settings. Valid custom bindings survive.
    useNativeConfig("cleanup");
    ShortcutSettings cleaned(
        Json::object({{"removedLegacyAction", "Meta+Y"},
                      {"closeWindowAlternate", "Meta+Q"},
                      {"launchTerminalAlternate", "Meta+Return"},
                      {"closeWindow", "Meta+T"}}));
    const auto saved = loadShortcutBindings();
    VERIFY(!saved.contains("removedLegacyAction"));
    VERIFY(!saved.contains("closeWindowAlternate"));
    VERIFY(!saved.contains("launchTerminalAlternate"));
    COMPARE((saved.at("closeWindow").get<std::string>()),
             std::string("Meta+T"));
    VERIFY(cleaned.actionFor(XKB_KEY_t, ShortcutMeta) == "closeWindow");
  }
};
} // namespace LunaDash
int main(int argc, char **argv) {
  return LunaDash::Test::run<LunaDash::WindowLayoutTests>(
      "WindowLayoutTests",
      {
      {"initTestCase", &LunaDash::WindowLayoutTests::initTestCase},
      {"layoutTemplateContract", &LunaDash::WindowLayoutTests::layoutTemplateContract},
      {"stackingRetainsIndependentGeometry", &LunaDash::WindowLayoutTests::stackingRetainsIndependentGeometry},
      {"fixedSplitsStayOnScreen", &LunaDash::WindowLayoutTests::fixedSplitsStayOnScreen},
      {"focusedSplitsPreserveMainPane", &LunaDash::WindowLayoutTests::focusedSplitsPreserveMainPane},
      {"focusedSplitUsesDestinationFocus", &LunaDash::WindowLayoutTests::focusedSplitUsesDestinationFocus},
      {"focusedSplitsPreferReadableTiles", &LunaDash::WindowLayoutTests::focusedSplitsPreferReadableTiles},
      {"manyWindowsAndOutputChanges", &LunaDash::WindowLayoutTests::manyWindowsAndOutputChanges},
      {"mixedLayoutEditsRemainBounded", &LunaDash::WindowLayoutTests::mixedLayoutEditsRemainBounded},
      {"singleWindowProportionsAndBounds", &LunaDash::WindowLayoutTests::singleWindowProportionsAndBounds},
      {"eightRowsAndOverflow", &LunaDash::WindowLayoutTests::eightRowsAndOverflow},
      {"swapsPreserveSlots", &LunaDash::WindowLayoutTests::swapsPreserveSlots},
      {"resizingAndSingleMovement", &LunaDash::WindowLayoutTests::resizingAndSingleMovement},
      {"workspaceMaximizePreservesTiling", &LunaDash::WindowLayoutTests::workspaceMaximizePreservesTiling},
      {"freeformSwapExchangesWindowSlots", &LunaDash::WindowLayoutTests::freeformSwapExchangesWindowSlots},
      {"scrollingColumnsKeepTheirWidth", &LunaDash::WindowLayoutTests::scrollingColumnsKeepTheirWidth},
      {"geometryBounds", &LunaDash::WindowLayoutTests::geometryBounds},
      {"windowSelectionScopeAndLifetime", &LunaDash::WindowLayoutTests::windowSelectionScopeAndLifetime},
      {"selectionLifecycle", &LunaDash::WindowLayoutTests::selectionLifecycle},
      {"externalFileManagerDefaults", &LunaDash::WindowLayoutTests::externalFileManagerDefaults},
      {"terminalAndShortcutMigration", &LunaDash::WindowLayoutTests::terminalAndShortcutMigration}},
      argc, argv);
}
