#include "compositor/render/renderer/WallpaperRenderer.hpp"

#include "compositor/render/element/ElementRender.hpp"
#include "compositor/render/renderer/WallpaperItem.hpp"
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>

namespace LuDash {

WallpaperRenderer::WallpaperRenderer(GraphicsApi api,
                                     std::shared_ptr<RenderState> state)
    : renderer_(api, std::move(state)),
      element_(makeRenderElement<WallpaperElement>()) {}

WallpaperRenderer::~WallpaperRenderer() {
  if (element_)
    element_->release();
}

bool WallpaperRenderer::initialize() {
  if (initialized_)
    return renderer_.ready();
  initialized_ = true;

  QString error;
  if (!renderer_.initialize(&error) || !element_->prepare(renderer_, &error)) {
    renderer_.state()->failed = true;
    qCritical().noquote() << "Wallpaper renderer initialization failed:"
                          << error;
    return false;
  }

  renderer_.state()->shaderReady = true;
  return true;
}

void WallpaperRenderer::synchronize(QQuickFramebufferObject *item) {
  element_->setPalette(static_cast<WallpaperItem *>(item)->palette());
}

void WallpaperRenderer::render() {
  if (!initialize() || renderer_.state()->failed)
    return;

  const QSize size = framebufferObject()->size();
  const ElementRenderContext context{
      QRect(0, 0, size.width(), size.height()), 1.0f};
  QString error;
  const bool ok = renderer_.render(*element_, context, &error);
  renderer_.state()->shaderReady = ok;
  renderer_.state()->failed = !ok;
  if (!ok)
    qWarning().noquote() << "Wallpaper render failed:" << error;
  QQuickOpenGLUtils::resetOpenGLState();
}

} // namespace LuDash
