#include "compositor/render/blur/BlurPass.hpp"

#include "compositor/render/renderer/Renderer.hpp"
#include <algorithm>
#include <cmath>

namespace LuDash {

bool BlurPass::prepare(Renderer &renderer, QString *error) {
  if (shader_)
    return true;
  shader_ = renderer.createProgram(
      {QStringLiteral("gl/fullscreen.vert"),
       QStringLiteral("blur/blur.frag")},
      error);
  return shader_ != nullptr;
}

bool BlurPass::resizeTargets(LuDashGLDispatch &gl, const QSize &size) {
  return targets_[0].resize(gl, size) && targets_[1].resize(gl, size);
}

void BlurPass::uploadKernel(int radius) {
  const float halfRadius = static_cast<float>(radius) * 0.5f;
  const float sigma = std::max(halfRadius / 3.0f, 0.5f);
  const int extent = radius / 2;

  float taps[17] = {1.0f};
  float total = 1.0f;
  for (int i = 1; i <= extent; ++i) {
    const float distance = static_cast<float>(i);
    taps[i] = std::exp(-0.5f * distance * distance / (sigma * sigma));
    total += 2.0f * taps[i];
  }

  float weights[9] = {1.0f / total};
  float offsets[9] = {0.0f};
  const int pairs = (extent + 1) / 2;
  for (int i = 1; i <= pairs; ++i) {
    const int first = 2 * i - 1;
    const float second =
        first + 1 <= extent ? taps[first + 1] : 0.0f;
    const float combined = taps[first] + second;
    weights[i] = combined / total;
    offsets[i] =
        combined > 0.0f ? static_cast<float>(first) + second / combined
                        : static_cast<float>(first);
  }

  auto &gl = shader_->gl();
  gl.Uniform1i(shader_->uniform("kernelPairs"), pairs);
  gl.Uniform1fv(shader_->uniform("weights[0]"), 9, weights);
  gl.Uniform1fv(shader_->uniform("offsets[0]"), 9, offsets);
}

bool BlurPass::draw(Renderer &renderer, const BlurRegion &region,
                    QString *error) {
  if (!prepare(renderer, error))
    return false;
  if (region.width <= 0 || region.height <= 0 || region.width > 32768 ||
      region.height > 32768 || region.x < 0 || region.y < 0 ||
      region.x > 32768 || region.y > 32768 || region.radius < 0 ||
      region.radius > 32 || !std::isfinite(region.opacity) ||
      region.opacity < 0.0f || region.opacity > 1.0f) {
    if (error)
      *error = QStringLiteral("invalid blur region");
    return false;
  }

  auto &gl = renderer.context().dispatch();
  GLint viewport[4]{};
  GLint drawTarget = 0;
  GLint readTarget = 0;
  gl.GetIntegerv(GL_VIEWPORT, viewport);
  gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawTarget);
  gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readTarget);

  const QSize halfSize(std::max(1, region.width / 2),
                       std::max(1, region.height / 2));
  if (!resizeTargets(gl, halfSize)) {
    if (error)
      *error = QStringLiteral("could not resize blur framebuffers");
    return false;
  }

  gl.ActiveTexture(GL_TEXTURE0);
  gl.Disable(GL_SCISSOR_TEST);
  gl.Disable(GL_STENCIL_TEST);
  gl.Disable(GL_DEPTH_TEST);
  gl.Disable(GL_CULL_FACE);
  gl.Disable(GL_BLEND);
  gl.DepthMask(GL_FALSE);
  gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  gl.BindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(drawTarget));
  targets_[0].bind(GL_DRAW_FRAMEBUFFER);
  gl.BlitFramebuffer(region.x, region.y, region.x + region.width,
                     region.y + region.height, 0, 0, halfSize.width(),
                     halfSize.height(), GL_COLOR_BUFFER_BIT, GL_LINEAR);

  targets_[1].bind();
  gl.Viewport(0, 0, halfSize.width(), halfSize.height());
  gl.BindTexture(GL_TEXTURE_2D, targets_[0].texture());
  shader_->bind();
  gl.Uniform1i(shader_->uniform("sourceTexture"), 0);
  gl.Uniform1f(shader_->uniform("opacity"), 1.0f);
  uploadKernel(region.radius);
  gl.Uniform2f(shader_->uniform("direction"),
               1.0f / static_cast<float>(halfSize.width()), 0.0f);
  shader_->drawFullscreen();

  targets_[0].bind();
  gl.BindTexture(GL_TEXTURE_2D, targets_[1].texture());
  gl.Uniform2f(shader_->uniform("direction"), 0.0f,
               1.0f / static_cast<float>(halfSize.height()));
  shader_->drawFullscreen();

  gl.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(drawTarget));
  gl.Viewport(region.x, region.y, region.width, region.height);
  gl.BindTexture(GL_TEXTURE_2D, targets_[0].texture());
  gl.Uniform1i(shader_->uniform("kernelPairs"), 0);
  gl.Uniform1f(shader_->uniform("weights[0]"), 1.0f);
  gl.Uniform1f(shader_->uniform("opacity"), region.opacity);
  gl.Enable(GL_BLEND);
  gl.BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  if (region.scissorEnabled) {
    gl.Enable(GL_SCISSOR_TEST);
    gl.Scissor(region.scissor[0], region.scissor[1], region.scissor[2],
               region.scissor[3]);
  }
  if (region.stencilEnabled) {
    gl.Enable(GL_STENCIL_TEST);
    gl.StencilFunc(GL_EQUAL, region.stencilValue, 0xff);
    gl.StencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  }
  shader_->drawFullscreen();
  shader_->unbind();

  gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawTarget));
  gl.BindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readTarget));
  gl.Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  return true;
}

void BlurPass::release() {
  targets_[0].reset();
  targets_[1].reset();
  shader_.reset();
}

} // namespace LuDash
