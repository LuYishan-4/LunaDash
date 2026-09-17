#pragma once
#include <QRectF>
#include <QSize>
namespace LuDash {
// Clip finite scene coordinates before converting them to framebuffer integers.
QRect blurViewportRegion(const QRectF& scene, qreal pixelRatio, const QSize& viewport);
}
