#pragma once

#include "compositor/renderer/opengl/Texture.hpp"

namespace LunaDash {

class Framebuffer final {
public:
  Framebuffer() = default;
  ~Framebuffer();

  Framebuffer(const Framebuffer &) = delete;
  Framebuffer &operator=(const Framebuffer &) = delete;

  bool resize(LuDashGLDispatch &gl, const QSize &size);
  void bind(GLenum target = GL_FRAMEBUFFER) const;
  void reset();
  GLuint id() const;
  GLuint texture() const;
  QSize size() const;
  bool valid() const;

private:
  LuDashGLDispatch *gl_ = nullptr;
  Texture color_;
  GLuint framebuffer_ = 0;
};

} // namespace LunaDash
