#pragma once

#include "compositor/renderer/RendererTypes.hpp"
#include <concepts>
#include <memory>
#include <string>
#include <utility>

namespace LunaDash {

class Renderer;

class ElementRender {
public:
  virtual ~ElementRender() = default;
  virtual bool prepare(Renderer &renderer, std::string *error = nullptr) = 0;
  virtual bool render(Renderer &renderer, const ElementRenderContext &context,
                      std::string *error = nullptr) = 0;
  virtual void release() = 0;
};

template <typename Element, typename... Args>
  requires std::derived_from<Element, ElementRender>
std::unique_ptr<Element> makeRenderElement(Args &&...args) {
  return std::make_unique<Element>(std::forward<Args>(args)...);
}

} // namespace LunaDash
