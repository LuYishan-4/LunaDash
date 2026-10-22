#include "compositor/renderer/blur/BlurGeometry.hpp"

#include <algorithm>
#include <cmath>

namespace LunaDash {

QRect blurViewportRegion(const QRectF &scene, qreal pixelRatio,
                         const QSize &viewport) {
  if (!scene.isValid() || viewport.isEmpty() || !std::isfinite(pixelRatio) ||
      pixelRatio <= 0)
    return {};

  const qreal left = scene.left() * pixelRatio;
  const qreal top = scene.top() * pixelRatio;
  const qreal right = scene.right() * pixelRatio;
  const qreal bottom = scene.bottom() * pixelRatio;
  if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(right) ||
      !std::isfinite(bottom))
    return {};

  const auto x1 = static_cast<int>(
      std::clamp(std::floor(left), qreal(0), qreal(viewport.width())));
  const auto y1 = static_cast<int>(
      std::clamp(std::floor(top), qreal(0), qreal(viewport.height())));
  const auto x2 = static_cast<int>(
      std::clamp(std::ceil(right), qreal(0), qreal(viewport.width())));
  const auto y2 = static_cast<int>(
      std::clamp(std::ceil(bottom), qreal(0), qreal(viewport.height())));
  if (x2 <= x1 || y2 <= y1)
    return {};
  return {x1, viewport.height() - y2, x2 - x1, y2 - y1};
}

} // namespace LunaDash
