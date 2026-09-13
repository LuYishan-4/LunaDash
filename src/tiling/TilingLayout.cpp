#include <LuDash/tiling/TilingLayout.h>
#include <LuDash/tiling_core/TilingGeometry.h>
#include <vector>
namespace LuDash {
QList<QRect> tileRectangles(QRect area, int count, double masterRatio, int gap) {
    if (count <= 0 || count > 4096) return {};
    std::vector<LuDashRectangle> rectangles(static_cast<size_t>(count));
    const auto size = ludash_tile_rectangles({area.x(), area.y(), area.width(), area.height()}, rectangles.size(), masterRatio, gap, rectangles.data(), rectangles.size());
    QList<QRect> result;
    result.reserve(static_cast<qsizetype>(size));
    for (size_t i = 0; i < size; ++i) { const auto& rectangle = rectangles[i]; result.append(QRect(rectangle.x, rectangle.y, rectangle.width, rectangle.height)); }
    return result;
}
}
