#pragma once
#include <LuDash/renderer/RenderBackend.h>
#include <QQuickFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLExtraFunctions>
namespace LuDash {
class WallpaperRenderer final : public QQuickFramebufferObject::Renderer {
public:
    WallpaperRenderer(GraphicsApi api, std::shared_ptr<RenderState> state);
    ~WallpaperRenderer() override;
    void render() override;
    void synchronize(QQuickFramebufferObject* item) override;
private:
    GraphicsApi api_;
    std::shared_ptr<RenderState> state_;
    std::unique_ptr<QOpenGLShaderProgram> program_;
    QOpenGLExtraFunctions* functions_ = nullptr;
    unsigned int vertexArray_ = 0;
    int palette_ = 0;
    bool initialized_ = false;
    bool initialize();
};
}
