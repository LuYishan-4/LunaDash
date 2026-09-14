#include <LuDash/tiling/TilingLayout.h>

#include <QRect>
#include <cstdio>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::fprintf(stderr, "Scrollable tiling check failed at line %d: %s\n",  \
                   __LINE__, #condition);                                      \
      return 1;                                                                \
    }                                                                          \
  } while (false)

int main() {
  using namespace LuDash;

  ScrollableTilingLayout layout(500, 10);
  CHECK(layout.insert(1, 101));
  CHECK(layout.insert(1, 102, 350));
  CHECK(layout.insert(1, 103));
  CHECK(layout.insert(1, 104, 275));
  CHECK(layout.insert(1, 105, 225));
  CHECK(layout.insert(1, 106, 190));

  CHECK(layout.groupWith(102, 101));
  CHECK(layout.groupWith(103, 101));
  CHECK(layout.groupWith(104, 101));
  CHECK(!layout.groupWith(101, 102));
  CHECK(!layout.groupWith(105, 101));

  auto grouped = layout.layout(1, QRect(0, 0, 900, 600));
  CHECK(grouped.size() == 6);
  for (int i = 0; i < 4; ++i) {
    CHECK(grouped[i].columnIndex == 0);
    CHECK(grouped[i].rowIndex == i);
    CHECK(grouped[i].columnMembers.size() == 4);
    CHECK(grouped[i].width == 500);
    CHECK(grouped[i].geometry.width() == 245);
    CHECK(grouped[i].geometry.height() == 295);
  }
  CHECK(grouped[0].geometry.x() == 0 && grouped[0].geometry.y() == 0);
  CHECK(grouped[1].geometry.x() == 255 && grouped[1].geometry.y() == 0);
  CHECK(grouped[2].geometry.x() == 0 && grouped[2].geometry.y() == 305);
  CHECK(grouped[3].geometry.x() == 255 && grouped[3].geometry.y() == 305);
  CHECK(grouped[4].columnIndex == 1 && grouped[4].width == 225);
  CHECK(grouped[5].columnIndex == 2 && grouped[5].width == 190);

  CHECK(layout.focus(101));
  CHECK(layout.focusDown(1));
  CHECK(layout.snapshot(1).focusedWindow == 102);
  CHECK(layout.focusDown(1));
  CHECK(layout.focusUp(1));
  CHECK(layout.snapshot(1).focusedWindow == 102);
  CHECK(layout.focusRight(1));
  CHECK(layout.snapshot(1).focusedWindow == 105);
  CHECK(layout.focusLeft(1));
  CHECK(layout.snapshot(1).focusedWindow == 101);

  CHECK(layout.setMinimized(103, true));
  grouped = layout.layout(1, QRect(0, 0, 900, 600));
  CHECK(grouped[2].minimized && grouped[2].rowIndex == -1);
  CHECK(grouped[2].geometry.isNull());
  CHECK(grouped[0].geometry.x() == 0 && grouped[0].geometry.y() == 0);
  CHECK(grouped[0].geometry.width() == 245 &&
        grouped[0].geometry.height() == 600);
  CHECK(grouped[1].geometry.x() == 255 && grouped[1].geometry.y() == 0);
  CHECK(grouped[1].geometry.width() == 245 &&
        grouped[1].geometry.height() == 295);
  CHECK(grouped[3].geometry.x() == 255 && grouped[3].geometry.y() == 305);
  CHECK(grouped[3].geometry.width() == 245 &&
        grouped[3].geometry.height() == 295);
  CHECK(!layout.groupWith(105, 101));
  CHECK(layout.setMinimized(103, false));
  CHECK(layout.snapshot(1).columns[4].width == 225);
  CHECK(layout.snapshot(1).columns[0].width == 500);

  CHECK(layout.resize(102, 420));
  const auto resized = layout.snapshot(1);
  CHECK(resized.columns[0].width == 420 && resized.columns[3].width == 420);
  CHECK(resized.columns[4].width == 225);
  CHECK(resized.columns[5].width == 190);

  CHECK(layout.expel(102));
  auto expelled = layout.snapshot(1);
  CHECK(expelled.columns[0].columnMembers.size() == 3);
  CHECK(expelled.columns[3].window == 102);
  CHECK(expelled.columns[3].columnIndex == 1);
  CHECK(expelled.columns[3].width == 420);
  CHECK(!layout.expel(102));

  CHECK(layout.reorder(101, 1));
  CHECK(layout.snapshot(1).columns[0].window == 102);
  CHECK(layout.snapshot(1).columns[1].window == 101);

  CHECK(layout.moveToWorkspace(105, 2));
  CHECK(layout.snapshot(2).columns.size() == 1);
  CHECK(layout.snapshot(2).columns[0].columnMembers.size() == 1);
  CHECK(layout.snapshot(2).columns[0].width == 225);
  CHECK(layout.remove(105));
  CHECK(layout.snapshot(2).columns.isEmpty());
  CHECK(!layout.remove(999));

  const auto compatibility = tileRectangles(QRect(0, 0, 1000, 700), 3, 0.5, 8);
  CHECK(compatibility.size() == 3);
  CHECK(compatibility[0].width() == compatibility[1].width());
  CHECK(compatibility[1].x() == compatibility[0].right() + 9);

  std::puts("Scrollable grouped tiling tests passed.");
  return 0;
}
