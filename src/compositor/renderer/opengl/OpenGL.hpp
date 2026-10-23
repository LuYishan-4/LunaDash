#pragma once

#include "compositor/renderer/opengl/GLDispatch.h"

#include "compositor/renderer/RendererTypes.hpp"
#include <memory>
#include <optional>
#include <string>

namespace LunaDash {

class OpenGL final {
public:
  explicit OpenGL(GraphicsApi requested = GraphicsApi::Auto,
                  std::shared_ptr<RenderState> state = {});

  bool initialize(std::string *error = nullptr);
  void shutdown();
  bool initialized() const;
  bool isOpenGLES() const;
  GraphicsApi requestedApi() const;
  LuDashGLDispatch &dispatch();
  const LuDashGLDispatch &dispatch() const;
  std::shared_ptr<RenderState> state() const;

  static LuDashGLProc resolve(const char *name);

private:
  GraphicsApi requested_ = GraphicsApi::Auto;
  std::shared_ptr<RenderState> state_;
  LuDashGLDispatch dispatch_{};
  bool initialized_ = false;
};

std::optional<GraphicsApi> graphicsApiFromArguments(int argc, char **argv);

} // namespace LunaDash
