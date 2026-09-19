#pragma once

#include "compositor/renderer/opengl/GLDispatch.h"
#include "compositor/renderer/opengl/ShaderAsset.hpp"

#include <QString>
#include <memory>

namespace LunaDash {

class Program final {
public:
  static std::unique_ptr<Program> create(const LuDashGLDispatch &dispatch,
                                         const QList<ShaderAsset> &assets,
                                         QString *error = nullptr);

  ~Program();

  Program(const Program &) = delete;
  Program &operator=(const Program &) = delete;

  LuDashGLDispatch &gl();
  const LuDashGLDispatch &gl() const;
  GLuint id() const;
  GLuint vertexArray() const;
  GLint uniform(const char *name) const;
  void bind() const;
  void unbind() const;
  void drawFullscreen() const;

private:
  explicit Program(const LuDashGLDispatch &dispatch);
  bool link(const QList<ShaderAsset> &assets, QString *error);

  LuDashGLDispatch gl_{};
  GLuint program_ = 0;
  GLuint vertexArray_ = 0;
};

} // namespace LunaDash
