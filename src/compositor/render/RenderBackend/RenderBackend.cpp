#include "compositor/render/RenderBackend/RenderBackend.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

int qInitResources_renderer_shaders();

namespace LuDash {
namespace {

ShaderStage stageFromToken(QString token) {
    token = token.trimmed().toLower();
    if (token == "vert" || token == "vertex" || token == "vsh" || token == "vs")
        return ShaderStage::Vertex;
    if (token == "frag" || token == "fragment" || token == "fsh" || token == "fs")
        return ShaderStage::Fragment;
    if (token == "geom" || token == "geometry" || token == "gsh" || token == "gs")
        return ShaderStage::Geometry;
    if (token == "comp" || token == "compute" || token == "csh" || token == "cs")
        return ShaderStage::Compute;
    if (token == "tesc" || token == "tesscontrol" || token == "tess_control")
        return ShaderStage::TessControl;
    if (token == "tese" || token == "tesseval" || token == "tess_eval")
        return ShaderStage::TessEvaluation;
    return ShaderStage::Unknown;
}

ShaderStage stageFromSourceDirective(const QByteArray& source) {
    const QList<QByteArray> lines = source.left(2048).split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        static const QByteArray pragma = "#pragma ludash_stage";
        if (!line.startsWith(pragma))
            continue;
        return stageFromToken(
            QString::fromLatin1(line.mid(pragma.size()).trimmed()));
    }
    return ShaderStage::Unknown;
}

QByteArray shaderVersionPrefix(ShaderStage stage, bool openGLES) {
    if (openGLES) {
        switch (stage) {
        case ShaderStage::Compute:
            return "#version 310 es\nprecision highp float;\nprecision highp int;\n";
        case ShaderStage::Geometry:
        case ShaderStage::TessControl:
        case ShaderStage::TessEvaluation:
            return "#version 320 es\nprecision highp float;\nprecision highp int;\n";
        case ShaderStage::Vertex:
        case ShaderStage::Fragment:
        case ShaderStage::Unknown:
            return "#version 300 es\nprecision highp float;\nprecision highp int;\n";
        }
    }

    switch (stage) {
    case ShaderStage::Compute:
        return "#version 430 core\n";
    case ShaderStage::TessControl:
    case ShaderStage::TessEvaluation:
        return "#version 400 core\n";
    case ShaderStage::Vertex:
    case ShaderStage::Fragment:
    case ShaderStage::Geometry:
    case ShaderStage::Unknown:
        return "#version 330 core\n";
    }
    return {};
}

bool sourceHasVersionDirective(const QByteArray& source) {
    QByteArray trimmed = source;
    if (trimmed.startsWith("\xEF\xBB\xBF"))
        trimmed.remove(0, 3);
    trimmed = trimmed.trimmed();
    return trimmed.startsWith("#version");
}

} // namespace

std::optional<GraphicsApi> graphicsApiFromArguments(int argc, char** argv) {
    QString value = "auto";
    for (int i = 1; i < argc; ++i) {
        const auto argument = QString::fromLocal8Bit(argv[i]);
        if (argument.startsWith("--graphics="))
            value = argument.mid(11);
        else if (argument == "--graphics") {
            if (i + 1 >= argc)
                return std::nullopt;
            value = QString::fromLocal8Bit(argv[++i]);
        }
    }
    if (value == "opengl")
        return GraphicsApi::OpenGL;
    if (value == "gles")
        return GraphicsApi::OpenGLES;
    if (value != "auto")
        return std::nullopt;
    return GraphicsApi::Auto;
}

void configureGraphics(GraphicsApi api) {
    ::qInitResources_renderer_shaders();
    const bool es =
        api == GraphicsApi::OpenGLES ||
        (api == GraphicsApi::Auto &&
         QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES);
    QSurfaceFormat format;
    format.setRenderableType(es ? QSurfaceFormat::OpenGLES
                                : QSurfaceFormat::OpenGL);
    format.setVersion(3, es ? 0 : 3);
    // Qt Wayland's external OES material supplies GLSL 120 on desktop GL.
    // Compatibility keeps that shader usable alongside our GLSL programs.
    format.setProfile(es ? QSurfaceFormat::NoProfile
                         : QSurfaceFormat::CompatibilityProfile);
    if (!es)
        format.setOption(QSurfaceFormat::DeprecatedFunctions);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
}

LuDashGLProc resolveGLFunction(const char* name) {
    auto* context = QOpenGLContext::currentContext();
    return context ? context->getProcAddress(name) : nullptr;
}

ShaderStage shaderStageFromName(const QString& name) {
    QString fileName = QFileInfo(name).fileName().toLower();
    QString suffix = QFileInfo(fileName).suffix();
    if (suffix == "glsl" || suffix == "shader") {
        fileName.chop(suffix.size() + 1);
        suffix = QFileInfo(fileName).suffix();
    }
    return stageFromToken(suffix);
}

GLenum shaderStageGlEnum(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Vertex:
        return GL_VERTEX_SHADER;
    case ShaderStage::Fragment:
        return GL_FRAGMENT_SHADER;
    case ShaderStage::Geometry:
        return GL_GEOMETRY_SHADER;
    case ShaderStage::Compute:
        return GL_COMPUTE_SHADER;
    case ShaderStage::TessControl:
        return GL_TESS_CONTROL_SHADER;
    case ShaderStage::TessEvaluation:
        return GL_TESS_EVALUATION_SHADER;
    case ShaderStage::Unknown:
        return 0;
    }
    return 0;
}

QByteArray shaderSource(const QString& name, bool openGLES) {
    QFile file(":/LuDash/data/shaders/" + name);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QByteArray source = file.readAll();
    ShaderStage stage = shaderStageFromName(name);
    if (stage == ShaderStage::Unknown)
        stage = stageFromSourceDirective(source);

    // Generic .glsl/.shader assets are intentionally accepted, but they need a
    // stage hint so LunaDash can choose a compatible GLSL version. Stage-
    // specific names such as effect.frag.glsl work without a pragma.
    const QString suffix = QFileInfo(name).suffix().toLower();
    if ((suffix == "glsl" || suffix == "shader") &&
        stage == ShaderStage::Unknown) {
        qWarning().noquote()
            << "Shader" << name
            << "needs a stage extension (for example .frag.glsl) or"
               "#pragma ludash_stage <vertex|fragment|geometry|compute|tesc|tese>";
        return {};
    }

    if (sourceHasVersionDirective(source))
        return source;
    return shaderVersionPrefix(stage, openGLES) + source;
}

} // namespace LuDash
