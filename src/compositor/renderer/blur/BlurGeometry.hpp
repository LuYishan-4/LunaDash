#pragma once

#include <QRectF>
#include <QSize>

namespace LunaDash {

QRect blurViewportRegion(const QRectF &scene, qreal pixelRatio,
                         const QSize &viewport);

} // namespace LunaDash
