#include "compositor/renderer/opengl/Framebuffer.hpp"

namespace LunaDash {

Framebuffer::~Framebuffer() { reset(); }

bool Framebuffer::resize(LuDashGLDispatch &gl, const QSize &size) {
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

void Framebuffer::bind(GLenum target) const {
  if (gl_)
    gl_->BindFramebuffer(target, framebuffer_);
}

void Framebuffer::reset() {
  if (gl_ && framebuffer_)
    gl_->DeleteFramebuffers(1, &framebuffer_);
  framebuffer_ = 0;
  color_.reset();
  gl_ = nullptr;
}

GLuint Framebuffer::id() const { return framebuffer_; }
GLuint Framebuffer::texture() const { return color_.texture(); }
QSize Framebuffer::size() const { return color_.size(); }
bool Framebuffer::valid() const { return framebuffer_ != 0 && color_.valid(); }

} // namespace LunaDash
