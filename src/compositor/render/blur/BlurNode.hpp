#pragma once

#include "compositor/render/blur/BlurItem.hpp"
#include "compositor/render/blur/BlurPass.hpp"
#include "compositor/render/renderer/Renderer.hpp"
#include <QSGRenderNode>
#include <memory>

namespace LuDash {

class BlurNode final : public QSGRenderNode {
public:
  explicit BlurNode(std::shared_ptr<BlurHealth> health);
  ~BlurNode() override;

  void synchronize(const QRectF &rectangle, int radius, qreal pixelRatio);
  void prepare() override;
  void render(const RenderState *state) override;
  void releaseResources() override;
  StateFlags changedStates() const override;
  RenderingFlags flags() const override;
  QRectF rect() const override;

private:
  bool initialize();

  QRectF rectangle_;
  QRectF sceneRectangle_;
  int radius_ = 18;
  qreal pixelRatio_ = 1;
  Renderer renderer_;
  std::unique_ptr<BlurPass> pass_;
  std::shared_ptr<BlurHealth> health_;
};

} // namespace LuDash
