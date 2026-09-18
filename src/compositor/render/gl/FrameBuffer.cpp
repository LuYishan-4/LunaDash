#include "compositor/render/gl/FrameBuffer.hpp"

namespace LuDash {

FrameBuffer::~FrameBuffer() { reset(); }

bool FrameBuffer::resize(LuDashGLDispatch &gl, const QSize &size) {
  if (valid() && gl_ == &gl && color_.size() == size)
    return true;

  reset();
  gl_ = &gl;
  if (!color_.resize(gl, size)) {
    gl_ = nullptr;
    return false;
  }

  gl.GenFramebuffers(1, &framebuffer_);
  if (!framebuffer_) {
    reset();
    return false;
  }

  gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                          color_.texture(), 0);
  if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    reset();
    return false;
  }
  return true;
}

void FrameBuffer::bind(GLenum target) const {
  if (gl_)
    gl_->BindFramebuffer(target, framebuffer_);
}

void FrameBuffer::reset() {
  if (gl_ && framebuffer_)
    gl_->DeleteFramebuffers(1, &framebuffer_);
  framebuffer_ = 0;
  color_.reset();
  gl_ = nullptr;
}

GLuint FrameBuffer::id() const { return framebuffer_; }
GLuint FrameBuffer::texture() const { return color_.texture(); }
QSize FrameBuffer::size() const { return color_.size(); }
bool FrameBuffer::valid() const {
  return framebuffer_ != 0 && color_.valid();
}

} // namespace LuDash
