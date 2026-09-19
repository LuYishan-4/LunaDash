#pragma once
#include "core/templates/Feature.hpp"
#include <utility>

namespace LunaDash::Templates {
// Composition keeps ownership explicit without virtual renderer hierarchies.
template <Feature Graphics> class RendererTemplate final {
public:
  template <typename... Args>
  explicit RendererTemplate(Args &&...args)
      : graphics_(std::forward<Args>(args)...) {}
  ~RendererTemplate() { graphics_.shutdown(); }
  RendererTemplate(const RendererTemplate &) = delete;
  RendererTemplate &operator=(const RendererTemplate &) = delete;

  template <typename... Args> bool initialize(Args &&...args) {
    return graphics_.initialize(std::forward<Args>(args)...);
  }
  Graphics &graphics() { return graphics_; }
  const Graphics &graphics() const { return graphics_; }

private:
  Graphics graphics_;
};
} // namespace LunaDash::Templates
