#pragma once

#include <QQuickItem>
#include <atomic>
#include <memory>

namespace LunaDash {

struct BlurHealth {
  std::atomic<bool> ready{false};
  std::atomic<bool> failed{false};
  std::atomic<unsigned int> frames{0};
};

class BlurItem final : public QQuickItem {
public:
  BlurItem(const std::shared_ptr<BlurHealth> &health, QQuickItem *parent);
  void setRadius(int radius);

protected:
  QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
  int radius_ = 18;
  std::shared_ptr<BlurHealth> health_;
};

} // namespace LunaDash
