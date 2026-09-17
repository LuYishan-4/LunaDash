#include "compositor/render/WallpaperRenderer/WallpaperRenderer.hpp"
#include "compositor/render/WallpaperItem/WallpaperItem.hpp"
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
namespace LuDash {
WallpaperRenderer::WallpaperRenderer(GraphicsApi api, std::shared_ptr<RenderState> state) : api_(api), state_(std::move(state)) {}
WallpaperRenderer::~WallpaperRenderer() { ludash_shader_destroy(program_); }
bool WallpaperRenderer::initialize() {
    initialized_ = true;
    auto* context = QOpenGLContext::currentContext();
    if (!context) { state_->failed = true; return false; }
    const bool es = context->isOpenGLES();
    const auto format = context->format();
    state_->isOpenGLES = es; state_->majorVersion = format.majorVersion(); state_->minorVersion = format.minorVersion();
    if ((api_ == GraphicsApi::OpenGL && es) || (api_ == GraphicsApi::OpenGLES && !es)
        || format.majorVersion() < 3 || (!es && format.majorVersion() == 3 && format.minorVersion() < 3)) {
        qCritical("Requested graphics API/context version is not available."); state_->failed = true; return false;
    }
    if (!es && format.profile() != QSurfaceFormat::CompatibilityProfile) {
        qCritical("Qt Wayland external textures require an OpenGL compatibility profile. Try --graphics gles.");
        state_->failed = true; return false;
    }
    char error[1024]{};
    const auto vertex = shaderSource("wallpaper.vert", es), fragment = shaderSource("wallpaper.frag", es);
    program_ = ludash_shader_create(resolveGLFunction, vertex.constData(), fragment.constData(), error, sizeof(error));
    if (!program_) { qCritical("Wallpaper shader failed: %s", error); state_->failed = true; return false; }
    qInfo().noquote() << "LuDash graphics context:" << (es ? "OpenGL ES" : "OpenGL") << format.majorVersion() << "." << format.minorVersion()
                     << (es ? "(ES profile)" : "(compatibility profile)");
    return true;
}
void WallpaperRenderer::synchronize(QQuickFramebufferObject* item) { palette_ = static_cast<WallpaperItem*>(item)->palette(); }
void WallpaperRenderer::render() {
    if ((!initialized_ && !initialize()) || state_->failed) return;
    const auto size = framebufferObject()->size();
    state_->shaderReady = ludash_wallpaper_draw(program_, size.width(), size.height(), palette_) != 0;
    if (!state_->shaderReady) state_->failed = true;
    QQuickOpenGLUtils::resetOpenGLState();
}
}
