#pragma once
#include "compositor/render/ShaderProgram/ShaderProgram.h"
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
// Takes ownership of shader, including when pass allocation fails.\nLuDashBlurPass* ludash_blur_create(LuDashShaderProgram* shader);
int ludash_blur_draw(LuDashBlurPass* pass, const LuDashBlurRegion* region);
void ludash_blur_destroy(LuDashBlurPass* pass);
#ifdef __cplusplus
}
}
#endif
