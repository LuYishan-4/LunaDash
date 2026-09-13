#include <LuDash/tiling/TilingLayout.h>
#include <algorithm>
namespace LuDash {
QList<QRect> tileRectangles(QRect area, int count, double masterRatio, int gap) {
    QList<QRect> result;
    if (count <= 0 || area.width() <= 0 || area.height() <= 0) return result;
    if (count == 1) return {area};
    gap = std::clamp(gap, 0, std::max(0, std::min(area.width() - 2, (area.height() - count + 1) / std::max(1, count - 2))));
    const int masterWidth = std::clamp(int((area.width() - gap) * masterRatio), 1, std::max(1, area.width() - gap - 1));
    result << QRect(area.x(), area.y(), masterWidth, area.height());
    const int stackCount = count - 1;
    const int stackHeight = area.height() - gap * (stackCount - 1);
    for (int i = 0; i < stackCount; ++i) {
        const int begin = i * stackHeight / stackCount;
        const int end = (i + 1) * stackHeight / stackCount;
        result << QRect(area.x() + masterWidth + gap, area.y() + begin + gap * i,
                        area.width() - masterWidth - gap, end - begin);
    }
    return result;
}
}
