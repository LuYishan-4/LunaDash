#include <LuDash/blur/BlurNode.h>
#include <LuDash/blur/BlurGeometry.h>
#include <LuDash/renderer/RenderBackend.h>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QQuickOpenGLUtils>
namespace LuDash {
BlurNode::BlurNode(std::shared_ptr<BlurHealth> health) : health_(std::move(health)) {}
BlurNode::~BlurNode() { releaseResources(); }
void BlurNode::synchronize(const QRectF& rectangle, int radius, qreal pixelRatio) { rectangle_ = rectangle; radius_ = radius; pixelRatio_ = pixelRatio; }
void BlurNode::prepare() {
    // Qt 6.4's RHI renderer supplies a stack-backed matrix during prepare().
    // Copy the mapped geometry while it is alive; render() runs later.
    sceneRectangle_ = matrix() ? matrix()->mapRect(rectangle_) : QRectF{};
}
QRectF BlurNode::rect() const { return rectangle_; }
QSGRenderNode::StateFlags BlurNode::changedStates() const { return ViewportState | ScissorState | RenderTargetState | BlendState | DepthState | StencilState | CullState; }
QSGRenderNode::RenderingFlags BlurNode::flags() const { return BoundedRectRendering; }
void BlurNode::releaseResources() { ludash_blur_destroy(pass_); pass_ = nullptr; }
bool BlurNode::initialize() {
    auto* context = QOpenGLContext::currentContext();
    if (!context) return false;
    char error[1024]{};
    const auto vertex = shaderSource("blur/blur.vert", context->isOpenGLES()), fragment = shaderSource("blur/blur.frag", context->isOpenGLES());
    pass_ = ludash_blur_create(resolveGLFunction, vertex.constData(), fragment.constData(), error, sizeof(error));
    if (!pass_) { qWarning("Backdrop blur shader failed: %s", error); health_->failed = true; }
    return pass_ != nullptr;
}
void BlurNode::render(const RenderState* state) {
    if (health_->failed || (!pass_ && !initialize())) return;
    auto* context = QOpenGLContext::currentContext();
    if (!context) { health_->failed = true; return; }
    int viewport[4]{};
    context->extraFunctions()->glGetIntegerv(GL_VIEWPORT, viewport);
    const QRect clipped = blurViewportRegion(sceneRectangle_, pixelRatio_, QSize(viewport[2], viewport[3]));
    if (clipped.isEmpty()) return;
    const QRect scissor = clipped.intersected(state->scissorRect());
    const LuDashBlurRegion parameters{clipped.x(), clipped.y(), clipped.width(), clipped.height(), radius_, static_cast<float>(inheritedOpacity()),
        state->scissorEnabled(), {scissor.x(), scissor.y(), scissor.width(), scissor.height()}, state->stencilEnabled(), state->stencilValue()};
    const bool rendered = ludash_blur_draw(pass_, &parameters) != 0;
    QQuickOpenGLUtils::resetOpenGLState();
    if (rendered) { health_->ready = true; ++health_->frames; } else health_->failed = true;
}
}
