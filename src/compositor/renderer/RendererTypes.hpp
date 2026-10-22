#pragma once

#include <QRect>
#include <atomic>

namespace LunaDash {
enum class GraphicsApi { Auto, OpenGL, OpenGLES };

struct RenderState {
  std::atomic<bool> shaderReady{false};
  std::atomic<bool> failed{false};
  std::atomic<bool> isOpenGLES{false};
  std::atomic<int> majorVersion{0};
  std::atomic<int> minorVersion{0};
};

struct ElementRenderContext {
  QRect viewport;
  float opacity = 1.0f;
};
} // namespace LunaDash
