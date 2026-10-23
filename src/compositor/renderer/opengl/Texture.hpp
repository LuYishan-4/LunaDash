#pragma once

#include "compositor/renderer/RendererTypes.hpp"
#include "compositor/renderer/opengl/GLDispatch.h"

namespace LunaDash {

class Texture final {
public:
  Texture() = default;
  ~Texture();

  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;

  bool resize(LuDashGLDispatch &gl, RenderSize size);
  void reset();
  GLuint texture() const;
  RenderSize size() const;
  bool valid() const;

private:
  LuDashGLDispatch *gl_ = nullptr;
  GLuint texture_ = 0;
  RenderSize size_;
};

} // namespace LunaDash
