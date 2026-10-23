#include "compositor/renderer/Renderer.hpp"

#include "compositor/renderer/element/ElementRender.hpp"
#include "compositor/renderer/opengl/ShaderAsset.hpp"

namespace LunaDash {

Renderer::Renderer(GraphicsApi api, std::shared_ptr<RenderState> state)
    : graphics_(api, std::move(state)) {}

bool Renderer::initialize(std::string *error) {
  return graphics_.initialize(error);
}
bool Renderer::ready() const { return graphics_.graphics().initialized(); }
ActiveGraphics &Renderer::context() { return graphics_.graphics(); }
const ActiveGraphics &Renderer::context() const { return graphics_.graphics(); }
std::shared_ptr<RenderState> Renderer::state() const {
  return graphics_.graphics().state();
}

std::unique_ptr<Program>
Renderer::createProgram(const std::vector<std::string> &assets,
                        std::string *error) {
  if (!initialize(error))
    return {};
  const auto sources = ShaderAssetLoader::loadMany(
      assets, graphics_.graphics().isOpenGLES(), error);
  if (sources.empty())
    return {};
  return Program::create(graphics_.graphics().dispatch(), sources, error);
}

bool Renderer::render(ElementRender &element,
                      const ElementRenderContext &renderContext,
                      std::string *error) {
  if (!initialize(error) || !element.prepare(*this, error))
    return false;
  return element.render(*this, renderContext, error);
}

} // namespace LunaDash
