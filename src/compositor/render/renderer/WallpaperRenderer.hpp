#pragma once

#include "compositor/render/gl/GLContext.hpp"
#include "compositor/render/renderer/Renderer.hpp"
#include "compositor/render/renderer/WallpaperElement.hpp"
#include <QQuickFramebufferObject>
#include <memory>

namespace LuDash {

class WallpaperRenderer final : public QQuickFramebufferObject::Renderer {
public:
  WallpaperRenderer(GraphicsApi api, std::shared_ptr<RenderState> state);
  ~WallpaperRenderer() override;

  void render() override;
  void synchronize(QQuickFramebufferObject *item) override;

private:
  LuDash::Renderer renderer_;
  std::unique_ptr<WallpaperElement> element_;
  bool initialized_ = false;
  bool initialize();
};

} // namespace LuDash
