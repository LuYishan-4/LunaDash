#pragma once

#include "compositor/renderer/element/ElementRender.hpp"
#include "compositor/renderer/opengl/Program.hpp"
#include <memory>

namespace LunaDash {

class DecorationElement final : public ElementRender {
public:
  void setOpacity(float opacity);

  bool prepare(Renderer &renderer, std::string *error = nullptr) override;
  bool render(Renderer &renderer, const ElementRenderContext &context,
              std::string *error = nullptr) override;
  void release() override;

private:
  std::unique_ptr<Program> program_;
  float opacity_ = 1.0f;
};

} // namespace LunaDash
