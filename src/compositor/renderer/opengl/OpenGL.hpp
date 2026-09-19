#pragma once

#include "compositor/renderer/opengl/GLDispatch.h"

#include "compositor/renderer/RendererTypes.hpp"
#include <QString>
#include <memory>
#include <optional>

class QOpenGLContext;

namespace LunaDash {

class OpenGL final {
public:
  explicit OpenGL(GraphicsApi requested = GraphicsApi::Auto,
                  std::shared_ptr<RenderState> state = {});

  bool initialize(QString *error = nullptr);
  void shutdown();
  bool initialized() const;
  bool isOpenGLES() const;
  GraphicsApi requestedApi() const;
  LuDashGLDispatch &dispatch();
  const LuDashGLDispatch &dispatch() const;
  std::shared_ptr<RenderState> state() const;

  static LuDashGLProc resolve(const char *name);
  static void configureDefault(GraphicsApi api);

private:
  GraphicsApi requested_ = GraphicsApi::Auto;
  std::shared_ptr<RenderState> state_;
  QOpenGLContext *context_ = nullptr;
  LuDashGLDispatch dispatch_{};
  bool initialized_ = false;
};

std::optional<GraphicsApi> graphicsApiFromArguments(int argc, char **argv);
void configureGraphics(GraphicsApi api);

} // namespace LunaDash
