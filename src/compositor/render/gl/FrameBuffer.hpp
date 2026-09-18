#pragma once

#include "compositor/render/gl/RenderBuffer.hpp"

namespace LuDash {

class FrameBuffer final {
public:
  FrameBuffer() = default;
  ~FrameBuffer();

  FrameBuffer(const FrameBuffer &) = delete;
  FrameBuffer &operator=(const FrameBuffer &) = delete;

  bool resize(LuDashGLDispatch &gl, const QSize &size);
  void bind(GLenum target = GL_FRAMEBUFFER) const;
  void reset();
  GLuint id() const;
  GLuint texture() const;
  QSize size() const;
  bool valid() const;

private:
  LuDashGLDispatch *gl_ = nullptr;
  RenderBuffer color_;
  GLuint framebuffer_ = 0;
};

} // namespace LuDash
