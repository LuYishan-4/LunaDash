#pragma once

#include "compositor/render/element/ElementRender.hpp"
#include "compositor/render/shader/ShaderProgram.hpp"
#include <memory>

namespace LuDash {

class WallpaperElement final : public ElementRender {
public:
  void setPalette(int palette);

  bool prepare(Renderer &renderer, QString *error = nullptr) override;
  bool render(Renderer &renderer, const ElementRenderContext &context,
              QString *error = nullptr) override;
  void release() override;

private:
  std::unique_ptr<ShaderProgram> program_;
  int palette_ = 0;
};

} // namespace LuDash
