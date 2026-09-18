#pragma once

#include "compositor/render/gl/GLDispatch.h"
#include <QSize>

namespace LuDash {

class RenderBuffer final {
public:
  RenderBuffer() = default;
  ~RenderBuffer();

  RenderBuffer(const RenderBuffer &) = delete;
  RenderBuffer &operator=(const RenderBuffer &) = delete;

  bool resize(LuDashGLDispatch &gl, const QSize &size);
  void reset();
  GLuint texture() const;
  QSize size() const;
  bool valid() const;

private:
  LuDashGLDispatch *gl_ = nullptr;
  GLuint texture_ = 0;
  QSize size_;
};

} // namespace LuDash
