#include "compositor/render/RenderBackend/RenderBackend.hpp"
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QFile>
#include <QCoreApplication>
int qInitResources_renderer_shaders();
namespace LuDash {
std::optional<GraphicsApi> graphicsApiFromArguments(int argc, char** argv) {
    QString value = "auto";
    for (int i = 1; i < argc; ++i) {
        const auto argument = QString::fromLocal8Bit(argv[i]);
        if (argument.startsWith("--graphics=")) value = argument.mid(11);
        else if (argument == "--graphics") {
            if (i + 1 >= argc) return std::nullopt;
            value = QString::fromLocal8Bit(argv[++i]);
        }
    }
    if (value == "opengl") return GraphicsApi::OpenGL;
    if (value == "gles") return GraphicsApi::OpenGLES;
    if (value != "auto") return std::nullopt;
    return GraphicsApi::Auto;
}
void configureGraphics(GraphicsApi api) {
    ::qInitResources_renderer_shaders();
    const bool es = api == GraphicsApi::OpenGLES || (api == GraphicsApi::Auto && QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES);
    QSurfaceFormat format;
    format.setRenderableType(es ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
    format.setVersion(3, es ? 0 : 3);
    // Qt Wayland's external OES material supplies GLSL 120 on desktop GL.
    // Compatibility keeps that shader usable alongside our GLSL 330 programs.
    format.setProfile(es ? QSurfaceFormat::NoProfile : QSurfaceFormat::CompatibilityProfile);
    if (!es) format.setOption(QSurfaceFormat::DeprecatedFunctions);
    // Qt Quick uses depth ordering for opaque items and stencil for clipping.
    format.setDepthBufferSize(24); format.setStencilBufferSize(8); format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);
    // EGLStream commits can arrive on the GUI thread before scene-graph sync.
    // Qt Wayland needs this share group to create its offscreen import context.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
}
LuDashGLProc resolveGLFunction(const char* name) {
    auto* context = QOpenGLContext::currentContext();
    return context ? context->getProcAddress(name) : nullptr;
}
QByteArray shaderSource(const QString& name, bool openGLES) {
    QFile file(":/LuDash/data/shaders/" + name);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QByteArray prefix = openGLES ? "#version 300 es\nprecision highp float;\nprecision highp int;\n" : "#version 330 core\n";
    return prefix + file.readAll();
}
}
