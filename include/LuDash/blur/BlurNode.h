#pragma once
#include <LuDash/blur/BlurItem.h>
#include <QSGRenderNode>
#include <LuDash/render_core/BlurPass.h>
namespace LuDash {
class BlurNode final : public QSGRenderNode {
public:
    explicit BlurNode(std::shared_ptr<BlurHealth> health);
    ~BlurNode() override;
    void synchronize(const QRectF& rectangle, int radius, qreal pixelRatio);
    void render(const RenderState* state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;
    RenderingFlags flags() const override;
    QRectF rect() const override;
private:
    bool initialize();
    QRectF rectangle_;
    int radius_ = 18;
    qreal pixelRatio_ = 1;
    LuDashBlurPass* pass_ = nullptr;
    std::shared_ptr<BlurHealth> health_;
};
}
