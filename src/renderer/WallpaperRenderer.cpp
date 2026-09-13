#include <LuDash/renderer/WallpaperRenderer.h>
#include <LuDash/renderer/WallpaperItem.h>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
namespace LuDash {
WallpaperRenderer::WallpaperRenderer(GraphicsApi api, std::shared_ptr<RenderState> state) : api_(api), state_(std::move(state)) {}
WallpaperRenderer::~WallpaperRenderer() {
    if (vertexArray_ && QOpenGLContext::currentContext()) functions_->glDeleteVertexArrays(1, &vertexArray_);
}
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
    functions_ = context->extraFunctions(); functions_->initializeOpenGLFunctions();
    program_ = std::make_unique<QOpenGLShaderProgram>();
    if (!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, shaderSource("wallpaper.vert", es))
        || !program_->addShaderFromSourceCode(QOpenGLShader::Fragment, shaderSource("wallpaper.frag", es)) || !program_->link()) {
        qCritical().noquote() << "Wallpaper shader failed:" << program_->log(); state_->failed = true; return false;
    }
    functions_->glGenVertexArrays(1, &vertexArray_);
    qInfo().noquote() << "LuDash graphics context:" << (es ? "OpenGL ES" : "OpenGL") << format.majorVersion() << "." << format.minorVersion();
    return true;
}
void WallpaperRenderer::synchronize(QQuickFramebufferObject* item) { palette_ = static_cast<WallpaperItem*>(item)->palette(); }
void WallpaperRenderer::render() {
    if ((!initialized_ && !initialize()) || state_->failed) return;
    const auto size = framebufferObject()->size();
    functions_->glViewport(0, 0, size.width(), size.height());
    functions_->glDisable(GL_DEPTH_TEST); functions_->glDisable(GL_SCISSOR_TEST); functions_->glDisable(GL_BLEND);
    program_->bind(); program_->setUniformValue("resolution", QVector2D(static_cast<float>(size.width()), static_cast<float>(size.height()))); program_->setUniformValue("palette", palette_);
    functions_->glBindVertexArray(vertexArray_); functions_->glDrawArrays(GL_TRIANGLES, 0, 3); functions_->glBindVertexArray(0);
    program_->release(); state_->shaderReady = true;
    QQuickOpenGLUtils::resetOpenGLState();
}
}
