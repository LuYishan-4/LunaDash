#pragma once

#include "compositor/render/gl/GLDispatch.h"

#include <QString>
#include <atomic>
#include <memory>
#include <optional>

class QOpenGLContext;

namespace LuDash {

enum class GraphicsApi { Auto, OpenGL, OpenGLES };

struct RenderState {
  std::atomic<bool> shaderReady{false};
  std::atomic<bool> failed{false};
  std::atomic<bool> isOpenGLES{false};
  std::atomic<int> majorVersion{0};
  std::atomic<int> minorVersion{0};
};

class GLContext final {
public:
  explicit GLContext(GraphicsApi requested = GraphicsApi::Auto,
                     std::shared_ptr<RenderState> state = {});

  bool initialize(QString *error = nullptr);
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

} // namespace LuDash
