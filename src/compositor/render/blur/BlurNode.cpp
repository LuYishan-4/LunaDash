#include "compositor/render/blur/BlurNode.hpp"

#include "compositor/render/blur/BlurGeometry.hpp"
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QQuickOpenGLUtils>

namespace LuDash {

BlurNode::BlurNode(std::shared_ptr<BlurHealth> health)
    : renderer_(GraphicsApi::Auto), health_(std::move(health)) {}

BlurNode::~BlurNode() { releaseResources(); }

void BlurNode::synchronize(const QRectF &rectangle, int radius,
                           qreal pixelRatio) {
  rectangle_ = rectangle;
  radius_ = radius;
  pixelRatio_ = pixelRatio;
}

void BlurNode::prepare() {
  sceneRectangle_ = matrix() ? matrix()->mapRect(rectangle_) : QRectF{};
}

QRectF BlurNode::rect() const { return rectangle_; }

QSGRenderNode::StateFlags BlurNode::changedStates() const {
  return ViewportState | ScissorState | RenderTargetState | BlendState |
         DepthState | StencilState | CullState;
}

QSGRenderNode::RenderingFlags BlurNode::flags() const {
  return BoundedRectRendering;
}

void BlurNode::releaseResources() {
  if (pass_)
    pass_->release();
  pass_.reset();
}

bool BlurNode::initialize() {
  QString error;
  if (!renderer_.initialize(&error)) {
    health_->failed = true;
    qWarning().noquote() << "Blur GL context failed:" << error;
    return false;
  }
  if (!pass_)
    pass_ = std::make_unique<BlurPass>();
  if (!pass_->prepare(renderer_, &error)) {
    health_->failed = true;
    qWarning().noquote() << "Backdrop blur shader failed:" << error;
    return false;
  }
  return true;
}

void BlurNode::render(const QSGRenderNode::RenderState *state) {
  if (health_->failed || (!pass_ && !initialize()))
    return;
  if (!renderer_.ready() && !initialize())
    return;

  auto *context = QOpenGLContext::currentContext();
  if (!context) {
    health_->failed = true;
    return;
  }

  int viewport[4]{};
  context->extraFunctions()->glGetIntegerv(GL_VIEWPORT, viewport);
  const QRect clipped =
      blurViewportRegion(sceneRectangle_, pixelRatio_,
                         QSize(viewport[2], viewport[3]));
  if (clipped.isEmpty())
    return;

  const QRect scissor = clipped.intersected(state->scissorRect());
  const BlurRegion parameters{
      clipped.x(),
      clipped.y(),
      clipped.width(),
      clipped.height(),
      radius_,
      static_cast<float>(inheritedOpacity()),
      state->scissorEnabled(),
      {scissor.x(), scissor.y(), scissor.width(), scissor.height()},
      state->stencilEnabled(),
      state->stencilValue(),
  };

  QString error;
  const bool rendered = pass_->draw(renderer_, parameters, &error);
  QQuickOpenGLUtils::resetOpenGLState();
  if (rendered) {
    health_->ready = true;
    ++health_->frames;
  } else {
    health_->failed = true;
    qWarning().noquote() << "Backdrop blur render failed:" << error;
  }
}

} // namespace LuDash
