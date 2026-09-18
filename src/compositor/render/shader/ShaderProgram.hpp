#pragma once

#include "compositor/render/gl/GLDispatch.h"
#include "compositor/render/shader/ShaderAsset.hpp"

#include <QString>
#include <memory>

namespace LuDash {

class ShaderProgram final {
public:
  static std::unique_ptr<ShaderProgram>
  create(const LuDashGLDispatch &dispatch, const QList<ShaderAsset> &assets,
         QString *error = nullptr);

  ~ShaderProgram();

  ShaderProgram(const ShaderProgram &) = delete;
  ShaderProgram &operator=(const ShaderProgram &) = delete;

  LuDashGLDispatch &gl();
  const LuDashGLDispatch &gl() const;
  GLuint id() const;
  GLuint vertexArray() const;
  GLint uniform(const char *name) const;
  void bind() const;
  void unbind() const;
  void drawFullscreen() const;

private:
  explicit ShaderProgram(const LuDashGLDispatch &dispatch);
  bool link(const QList<ShaderAsset> &assets, QString *error);

  LuDashGLDispatch gl_{};
  GLuint program_ = 0;
  GLuint vertexArray_ = 0;
};

} // namespace LuDash
