#pragma once

#include "compositor/renderer/RendererConfig.hpp"
#include "compositor/renderer/opengl/Program.hpp"
#include "core/templates/Renderer.hpp"

#include <memory>
#include <string>
#include <vector>

namespace LunaDash {

class ElementRender;
struct ElementRenderContext;

class Renderer final {
public:
  explicit Renderer(GraphicsApi api = GraphicsApi::Auto,
                    std::shared_ptr<RenderState> state = {});

  bool initialize(std::string *error = nullptr);
  bool ready() const;

  ActiveGraphics &context();
  const ActiveGraphics &context() const;
  std::shared_ptr<RenderState> state() const;

  std::unique_ptr<Program> createProgram(const std::vector<std::string> &assets,
                                         std::string *error = nullptr);

  bool render(ElementRender &element, const ElementRenderContext &context,
              std::string *error = nullptr);

private:
  Templates::RendererTemplate<ActiveGraphics> graphics_;
};

} // namespace LunaDash
