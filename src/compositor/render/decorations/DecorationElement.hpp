#pragma once

#include "compositor/render/element/ElementRender.hpp"
#include "compositor/render/shader/ShaderProgram.hpp"
#include <memory>

namespace LuDash {

class DecorationElement final : public ElementRender {
public:
  void setOpacity(float opacity);

  bool prepare(Renderer &renderer, QString *error = nullptr) override;
  bool render(Renderer &renderer, const ElementRenderContext &context,
              QString *error = nullptr) override;
  void release() override;

private:
  std::unique_ptr<ShaderProgram> program_;
  float opacity_ = 1.0f;
};

} // namespace LuDash
