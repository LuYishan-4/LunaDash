#pragma once
#include <LuDash/renderer/RenderBackend.h>
#include <QQuickFramebufferObject>
#include <LuDash/render_core/ShaderProgram.h>
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
    LuDashShaderProgram* program_ = nullptr;
    int palette_ = 0;
    bool initialized_ = false;
    bool initialize();
};
}
