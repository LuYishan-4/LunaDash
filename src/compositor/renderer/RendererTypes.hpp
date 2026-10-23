#pragma once

#include <atomic>

namespace LunaDash {
enum class GraphicsApi { Auto, OpenGL, OpenGLES };

struct RenderSize {
  int width = 0;
  int height = 0;

  bool empty() const { return width <= 0 || height <= 0; }
  friend bool operator==(const RenderSize &, const RenderSize &) = default;
};

struct RenderViewport {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  bool empty() const { return width <= 0 || height <= 0; }
};

struct RenderState {
  std::atomic<bool> shaderReady{false};
  std::atomic<bool> failed{false};
  std::atomic<bool> isOpenGLES{false};
  std::atomic<int> majorVersion{0};
  std::atomic<int> minorVersion{0};
};

struct ElementRenderContext {
  RenderViewport viewport;
  float opacity = 1.0f;
};
} // namespace LunaDash
