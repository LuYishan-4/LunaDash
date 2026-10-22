#pragma once

#include "compositor/renderer/Renderer.hpp"
#include "compositor/renderer/blur/BlurItem.hpp"
#include "compositor/renderer/opengl/blur/BlurPass.hpp"
#include <QSGRenderNode>
#include <memory>

namespace LunaDash {

class BlurNode final : public QSGRenderNode {
public:
  explicit BlurNode(std::shared_ptr<BlurHealth> health);
  ~BlurNode() override;

  void synchronize(const QRectF &rectangle, int radius, qreal pixelRatio);
  void prepare() override;
  void render(const QSGRenderNode::RenderState *state) override;
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

} // namespace LunaDash
