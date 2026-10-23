#pragma once

#include "compositor/renderer/opengl/Framebuffer.hpp"
#include "compositor/renderer/opengl/Program.hpp"
#include <array>
#include <memory>
#include <string>

namespace LunaDash {

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
  bool prepare(Renderer &renderer, std::string *error = nullptr);
  bool draw(Renderer &renderer, const BlurRegion &region,
            std::string *error = nullptr);
  void release();

private:
  bool resizeTargets(LuDashGLDispatch &gl, RenderSize size);
  void uploadKernel(int radius);

  std::unique_ptr<Program> shader_;
  std::array<Framebuffer, 2> targets_;
};

} // namespace LunaDash
