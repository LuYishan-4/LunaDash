#include "compositor/render/renderer/Renderer.hpp"

#include "compositor/render/element/ElementRender.hpp"
#include "compositor/render/shader/ShaderAsset.hpp"

namespace LuDash {

Renderer::Renderer(GraphicsApi api, std::shared_ptr<RenderState> state)
    : context_(api, std::move(state)) {}

bool Renderer::initialize(QString *error) { return context_.initialize(error); }
bool Renderer::ready() const { return context_.initialized(); }
GLContext &Renderer::context() { return context_; }
const GLContext &Renderer::context() const { return context_; }
std::shared_ptr<RenderState> Renderer::state() const { return context_.state(); }

std::unique_ptr<ShaderProgram>
Renderer::createProgram(const QStringList &assets, QString *error) {
  if (!initialize(error))
    return {};
  const auto sources =
      ShaderAssetLoader::loadMany(assets, context_.isOpenGLES(), error);
  if (sources.isEmpty())
    return {};
  return ShaderProgram::create(context_.dispatch(), sources, error);
}

bool Renderer::render(ElementRender &element,
                      const ElementRenderContext &renderContext,
                      QString *error) {
  if (!initialize(error) || !element.prepare(*this, error))
    return false;
  return element.render(*this, renderContext, error);
}

} // namespace LuDash
