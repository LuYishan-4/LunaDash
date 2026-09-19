#pragma once
#include "compositor/renderer/opengl/GLDispatch.h"
#include "compositor/renderer/opengl/ShaderAsset.hpp"
#include <memory>

namespace LunaDash {
class Shader final {
public:
  static std::unique_ptr<Shader> compile(const LuDashGLDispatch &dispatch,
                                         const ShaderAsset &asset,
                                         QString *error);
  ~Shader();
  Shader(const Shader &) = delete;
  Shader &operator=(const Shader &) = delete;
  GLuint id() const;

private:
  Shader(const LuDashGLDispatch &dispatch, GLuint id);
  LuDashGLDispatch gl_{};
  GLuint id_ = 0;
};
} // namespace LunaDash
