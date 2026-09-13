#pragma once
#include <QString>
#include <QByteArray>
#include <atomic>
#include <memory>
#include <optional>
namespace LuDash {
enum class GraphicsApi { Auto, OpenGL, OpenGLES };
struct RenderState {
    std::atomic<bool> shaderReady{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> isOpenGLES{false};
    std::atomic<int> majorVersion{0};
    std::atomic<int> minorVersion{0};
};
std::optional<GraphicsApi> graphicsApiFromArguments(int argc, char** argv);
void configureGraphics(GraphicsApi api);
QByteArray shaderSource(const QString& name, bool openGLES);
}
