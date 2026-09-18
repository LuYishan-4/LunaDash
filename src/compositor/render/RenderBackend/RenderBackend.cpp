#include "compositor/render/RenderBackend/RenderBackend.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <vector>

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

struct LoadedShaderAsset {
    ShaderStage stage = ShaderStage::Unknown;
    QByteArray source;
};

std::optional<LoadedShaderAsset> loadShaderAsset(const QString& name,
                                                 bool openGLES) {
    QFile file(":/LuDash/data/shaders/" + name);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;

    QByteArray source = file.readAll();
    ShaderStage stage = shaderStageFromName(name);
    if (stage == ShaderStage::Unknown)
        stage = stageFromSourceDirective(source);

    const QString suffix = QFileInfo(name).suffix().toLower();
    if ((suffix == "glsl" || suffix == "shader") &&
        stage == ShaderStage::Unknown) {
        qWarning().noquote()
            << "Shader" << name
            << "needs a stage extension (for example .frag.glsl) or"
               "#pragma ludash_stage <vertex|fragment|geometry|compute|tesc|tese>";
        return std::nullopt;
    }
    if (stage == ShaderStage::Unknown) {
        qWarning().noquote() << "Shader" << name
                             << "has an unrecognized OpenGL stage suffix";
        return std::nullopt;
    }

    if (!sourceHasVersionDirective(source))
        source.prepend(shaderVersionPrefix(stage, openGLES));
    return LoadedShaderAsset{stage, source};
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
    const auto asset = loadShaderAsset(name, openGLES);
    return asset ? asset->source : QByteArray{};
}

LuDashShaderProgram* shaderProgramFromAssets(const QStringList& names,
                                             bool openGLES,
                                             QString* error) {
    if (error)
        error->clear();
    if (names.isEmpty()) {
        if (error)
            *error = QStringLiteral("shader program has no asset files");
        return nullptr;
    }

    std::vector<LoadedShaderAsset> assets;
    assets.reserve(static_cast<size_t>(names.size()));
    for (const auto& name : names) {
        const auto asset = loadShaderAsset(name, openGLES);
        if (!asset) {
            if (error)
                *error = QStringLiteral("could not load shader asset: %1").arg(name);
            return nullptr;
        }
        assets.push_back(*asset);
    }

    std::vector<LuDashShaderSource> stages;
    stages.reserve(assets.size());
    for (const auto& asset : assets) {
        const GLenum stage = shaderStageGlEnum(asset.stage);
        if (!stage) {
            if (error)
                *error = QStringLiteral("shader asset has no OpenGL stage");
            return nullptr;
        }
        stages.push_back({stage, asset.source.constData()});
    }

    char compileError[4096]{};
    auto* program = ludash_shader_create_stages(
        resolveGLFunction, stages.data(), stages.size(), compileError,
        sizeof(compileError));
    if (!program && error)
        *error = QString::fromLocal8Bit(compileError);
    return program;
}

} // namespace LuDash
