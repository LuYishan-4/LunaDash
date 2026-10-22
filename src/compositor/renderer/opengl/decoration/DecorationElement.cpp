#include "compositor/renderer/opengl/decoration/DecorationElement.hpp"

#include "compositor/renderer/Renderer.hpp"
#include <algorithm>

namespace LunaDash {

void DecorationElement::setOpacity(float opacity) {
  opacity_ = std::clamp(opacity, 0.0f, 1.0f);
}

bool DecorationElement::prepare(Renderer &renderer, QString *error) {
  if (program_)
    return true;
  program_ = renderer.createProgram(
      {QStringLiteral("Fullscreen.vert"), QStringLiteral("Decoration.frag")},
      error);
  return program_ != nullptr;
}

bool DecorationElement::render(Renderer &, const ElementRenderContext &context,
                               QString *error) {
  if (!program_) {
    if (error)
      *error = QStringLiteral("decoration element is not prepared");
    return false;
  }
  if (context.viewport.isEmpty())
    return true;

  auto &gl = program_->gl();
  gl.Viewport(context.viewport.x(), context.viewport.y(),
              context.viewport.width(), context.viewport.height());
  gl.Enable(GL_BLEND);
  gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  program_->bind();
  gl.Uniform1f(program_->uniform("opacity"),
               std::clamp(context.opacity * opacity_, 0.0f, 1.0f));
  program_->drawFullscreen();
  program_->unbind();
  return true;
}

void DecorationElement::release() { program_.reset(); }

} // namespace LunaDash
