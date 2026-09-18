#pragma once

#include "compositor/render/gl/GLContext.hpp"
#include <QQuickFramebufferObject>
#include <memory>

namespace LuDash {

class WallpaperItem final : public QQuickFramebufferObject {
public:
  WallpaperItem(GraphicsApi api, std::shared_ptr<RenderState> state,
                QQuickItem *parent);
  QQuickFramebufferObject::Renderer *createRenderer() const override;

  void setPalette(int palette);
  int palette() const;

private:
  GraphicsApi api_;
  std::shared_ptr<RenderState> state_;
  int palette_ = 0;
};

} // namespace LuDash
