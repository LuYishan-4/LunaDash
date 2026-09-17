#pragma once
#include "compositor/render/RenderBackend/RenderBackend.hpp"
#include <QQuickFramebufferObject>
#include "compositor/render/ShaderProgram/ShaderProgram.h"
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
