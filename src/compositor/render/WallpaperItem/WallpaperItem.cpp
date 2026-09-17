#include "compositor/render/WallpaperItem/WallpaperItem.hpp"
#include "compositor/render/WallpaperRenderer/WallpaperRenderer.hpp"
namespace LuDash {
WallpaperItem::WallpaperItem(GraphicsApi api, std::shared_ptr<RenderState> state, QQuickItem* parent)
    : QQuickFramebufferObject(parent), api_(api), state_(std::move(state)) {
    setMirrorVertically(true);
    setAcceptedMouseButtons(Qt::NoButton);
}
QQuickFramebufferObject::Renderer* WallpaperItem::createRenderer() const { return new WallpaperRenderer(api_, state_); }
void WallpaperItem::setPalette(int palette) { if (palette_ != palette) { palette_ = palette; update(); } }
int WallpaperItem::palette() const { return palette_; }
}
