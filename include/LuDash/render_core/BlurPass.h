#pragma once
#include <LuDash/render_core/ShaderProgram.h>
#ifdef __cplusplus
namespace LuDash {
extern "C" {
#endif
typedef struct LuDashBlurPass LuDashBlurPass;
typedef struct LuDashBlurRegion {
    int x, y, width, height;
    int radius;
    float opacity;
    int scissor_enabled;
    int scissor[4];
    int stencil_enabled;
    int stencil_value;
} LuDashBlurRegion;
LuDashBlurPass* ludash_blur_create(LuDashGLResolver resolve, const char* vertex, const char* fragment, char* error, size_t capacity);
int ludash_blur_draw(LuDashBlurPass* pass, const LuDashBlurRegion* region);
void ludash_blur_destroy(LuDashBlurPass* pass);
#ifdef __cplusplus
}
}
#endif
