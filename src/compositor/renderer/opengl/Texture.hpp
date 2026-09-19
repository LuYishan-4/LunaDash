#pragma once

#include "compositor/renderer/opengl/GLDispatch.h"
#include <QSize>

namespace LunaDash {

class Texture final {
public:
  Texture() = default;
  ~Texture();

  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;

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

} // namespace LunaDash
