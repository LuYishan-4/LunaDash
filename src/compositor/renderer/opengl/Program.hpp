#pragma once

#include "compositor/renderer/opengl/GLDispatch.h"
#include "compositor/renderer/opengl/ShaderAsset.hpp"

#include <memory>
#include <string>
#include <vector>

namespace LunaDash {

class Program final {
public:
  static std::unique_ptr<Program> create(const LuDashGLDispatch &dispatch,
                                         const std::vector<ShaderAsset> &assets,
                                         std::string *error = nullptr);

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
  bool link(const std::vector<ShaderAsset> &assets, std::string *error);

  LuDashGLDispatch gl_{};
  GLuint program_ = 0;
  GLuint vertexArray_ = 0;
};

} // namespace LunaDash
