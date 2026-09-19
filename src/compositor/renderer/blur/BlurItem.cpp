#include "compositor/renderer/blur/BlurItem.hpp"

#include "compositor/renderer/opengl/blur/BlurNode.hpp"
#include <QQuickWindow>
#include <algorithm>

namespace LunaDash {

BlurItem::BlurItem(const std::shared_ptr<BlurHealth> &health,
                   QQuickItem *parent)
    : QQuickItem(parent), health_(health) {
  setFlag(ItemHasContents);
  setAcceptedMouseButtons(Qt::NoButton);
  setZ(-1);
}

void BlurItem::setRadius(int radius) {
  radius_ = std::clamp(radius, 0, 32);
  update();
}

QSGNode *BlurItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
  if (!radius_ || width() <= 0 || height() <= 0) {
    delete oldNode;
    return nullptr;
  }
  auto *node = static_cast<BlurNode *>(oldNode);
  if (!node)
    node = new BlurNode(health_);
  node->synchronize(boundingRect(), radius_,
                    window() ? window()->devicePixelRatio() : 1);
  return node;
}

} // namespace LunaDash
