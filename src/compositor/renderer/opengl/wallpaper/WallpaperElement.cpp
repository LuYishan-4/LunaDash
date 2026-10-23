#include "compositor/renderer/opengl/wallpaper/WallpaperElement.hpp"

#include "compositor/renderer/Renderer.hpp"

namespace LunaDash {

void WallpaperElement::setPalette(int palette) { palette_ = palette; }

bool WallpaperElement::prepare(Renderer &renderer, std::string *error) {
  if (program_)
    return true;
  program_ =
      renderer.createProgram({"Fullscreen.vert", "Wallpaper.frag"}, error);
  return program_ != nullptr;
}

bool WallpaperElement::render(Renderer &, const ElementRenderContext &context,
                              std::string *error) {
  if (!program_) {
    if (error)
      *error = "wallpaper element is not prepared";
    return false;
  }
  if (context.viewport.width <= 0 || context.viewport.height <= 0)
    return true;

  auto &gl = program_->gl();
  gl.Viewport(context.viewport.x, context.viewport.y, context.viewport.width,
              context.viewport.height);
  gl.Disable(GL_DEPTH_TEST);
  gl.Disable(GL_SCISSOR_TEST);
  gl.Disable(GL_BLEND);
  program_->bind();
  gl.Uniform2f(program_->uniform("resolution"),
               static_cast<float>(context.viewport.width),
               static_cast<float>(context.viewport.height));
  gl.Uniform1i(program_->uniform("palette"), palette_);
  program_->drawFullscreen();
  program_->unbind();
  return true;
}

void WallpaperElement::release() { program_.reset(); }

} // namespace LunaDash
