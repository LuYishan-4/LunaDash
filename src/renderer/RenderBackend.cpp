#include <LuDash/renderer/RenderBackend.h>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QFile>
int qInitResources_renderer_shaders();
namespace LuDash {
GraphicsApi graphicsApiFromArguments(int argc, char** argv) {
    QString value = "auto";
    for (int i = 1; i < argc; ++i) {
        const auto argument = QString::fromLocal8Bit(argv[i]);
        if (argument.startsWith("--graphics=")) value = argument.mid(11);
        else if (argument == "--graphics" && i + 1 < argc) value = QString::fromLocal8Bit(argv[++i]);
    }
    if (value == "opengl") return GraphicsApi::OpenGL;
    if (value == "gles") return GraphicsApi::OpenGLES;
    if (value != "auto") qFatal("--graphics must be auto, opengl, or gles");
    return GraphicsApi::Auto;
}
void configureGraphics(GraphicsApi api) {
    ::qInitResources_renderer_shaders();
    const bool es = api == GraphicsApi::OpenGLES || (api == GraphicsApi::Auto && QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES);
    QSurfaceFormat format;
    format.setRenderableType(es ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
    format.setVersion(3, es ? 0 : 3);
    format.setProfile(es ? QSurfaceFormat::NoProfile : QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(0); format.setStencilBufferSize(0); format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
}
QByteArray shaderSource(const QString& name, bool openGLES) {
    QFile file(":/LuDash/data/shaders/" + name);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QByteArray prefix = openGLES ? "#version 300 es\nprecision highp float;\nprecision highp int;\n" : "#version 330 core\n";
    return prefix + file.readAll();
}
}
