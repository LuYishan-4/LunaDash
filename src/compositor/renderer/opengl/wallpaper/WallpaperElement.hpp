#pragma once

#include "compositor/renderer/element/ElementRender.hpp"
#include "compositor/renderer/opengl/Program.hpp"
#include <memory>

namespace LunaDash {

class WallpaperElement final : public ElementRender {
public:
  void setPalette(int palette);

  bool prepare(Renderer &renderer, std::string *error = nullptr) override;
  bool render(Renderer &renderer, const ElementRenderContext &context,
              std::string *error = nullptr) override;
  void release() override;

private:
  std::unique_ptr<Program> program_;
  int palette_ = 0;
};

} // namespace LunaDash
