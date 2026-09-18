#pragma once

#include <QRectF>
#include <QSize>

namespace LuDash {

QRect blurViewportRegion(const QRectF &scene, qreal pixelRatio,
                         const QSize &viewport);

} // namespace LuDash
