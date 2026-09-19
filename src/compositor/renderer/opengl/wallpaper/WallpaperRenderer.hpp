#pragma once

#include "compositor/renderer/Renderer.hpp"
#include "compositor/renderer/opengl/OpenGL.hpp"
#include "compositor/renderer/opengl/wallpaper/WallpaperElement.hpp"
#include <QQuickFramebufferObject>
#include <memory>

namespace LunaDash {

class WallpaperRenderer final : public QQuickFramebufferObject::Renderer {
public:
  WallpaperRenderer(GraphicsApi api, std::shared_ptr<RenderState> state);
  ~WallpaperRenderer() override;

  void render() override;
  void synchronize(QQuickFramebufferObject *item) override;

private:
  LunaDash::Renderer renderer_;
  std::unique_ptr<WallpaperElement> element_;
  bool initialized_ = false;
  bool initialize();
};

} // namespace LunaDash
