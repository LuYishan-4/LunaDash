#pragma once

#include "compositor/render/gl/GLContext.hpp"
#include "compositor/render/shader/ShaderProgram.hpp"

#include <QStringList>
#include <memory>

namespace LuDash {

class ElementRender;
struct ElementRenderContext;

class Renderer final {
public:
  explicit Renderer(GraphicsApi api = GraphicsApi::Auto,
                    std::shared_ptr<RenderState> state = {});

  bool initialize(QString *error = nullptr);
  bool ready() const;

  GLContext &context();
  const GLContext &context() const;
  std::shared_ptr<RenderState> state() const;

  std::unique_ptr<ShaderProgram>
  createProgram(const QStringList &assets, QString *error = nullptr);

  bool render(ElementRender &element, const ElementRenderContext &context,
              QString *error = nullptr);

private:
  GLContext context_;
};

} // namespace LuDash
