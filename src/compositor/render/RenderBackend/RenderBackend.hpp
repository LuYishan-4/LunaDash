#pragma once
#include <QString>
#include "compositor/render/GLDispatch/GLDispatch.h"
#include <QByteArray>
#include <atomic>
#include <memory>
#include <optional>

namespace LuDash {

enum class GraphicsApi { Auto, OpenGL, OpenGLES };

enum class ShaderStage {
    Unknown,
    Vertex,
    Fragment,
    Geometry,
    Compute,
    TessControl,
    TessEvaluation,
};

struct RenderState {
    std::atomic<bool> shaderReady{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> isOpenGLES{false};
    std::atomic<int> majorVersion{0};
    std::atomic<int> minorVersion{0};
};

std::optional<GraphicsApi> graphicsApiFromArguments(int argc, char** argv);
void configureGraphics(GraphicsApi api);
LuDashGLProc resolveGLFunction(const char* name);

ShaderStage shaderStageFromName(const QString& name);
GLenum shaderStageGlEnum(ShaderStage stage);
QByteArray shaderSource(const QString& name, bool openGLES);

}
