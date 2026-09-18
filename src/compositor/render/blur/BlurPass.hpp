#pragma once

#include "compositor/render/gl/FrameBuffer.hpp"
#include "compositor/render/shader/ShaderProgram.hpp"
#include <array>
#include <memory>

namespace LuDash {

class Renderer;

struct BlurRegion {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  int radius = 0;
  float opacity = 1.0f;
  bool scissorEnabled = false;
  std::array<int, 4> scissor{0, 0, 0, 0};
  bool stencilEnabled = false;
  int stencilValue = 0;
};

class BlurPass final {
public:
  bool prepare(Renderer &renderer, QString *error = nullptr);
  bool draw(Renderer &renderer, const BlurRegion &region,
            QString *error = nullptr);
  void release();

private:
  bool resizeTargets(LuDashGLDispatch &gl, const QSize &size);
  void uploadKernel(int radius);

  std::unique_ptr<ShaderProgram> shader_;
  std::array<FrameBuffer, 2> targets_;
};

} // namespace LuDash
