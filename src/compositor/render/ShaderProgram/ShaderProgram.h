#pragma once
#include "compositor/render/GLDispatch/GLDispatch.h"
#include <stddef.h>

#ifdef __cplusplus
namespace LuDash {
extern "C" {
#endif

typedef struct LuDashShaderSource {
    GLenum stage;
    const char* source;
} LuDashShaderSource;

typedef struct LuDashShaderProgram {
    LuDashGLDispatch gl;
    GLuint program;
    GLuint vertex_array;
} LuDashShaderProgram;

LuDashShaderProgram* ludash_shader_create_stages(
    LuDashGLResolver resolve,
    const LuDashShaderSource* stages,
    size_t stage_count,
    char* error,
    size_t capacity);

LuDashShaderProgram* ludash_shader_create(
    LuDashGLResolver resolve,
    const char* vertex,
    const char* fragment,
    char* error,
    size_t capacity);

void ludash_shader_destroy(LuDashShaderProgram* shader);
int ludash_wallpaper_draw(LuDashShaderProgram* shader, int width, int height, int palette);

#ifdef __cplusplus
}
}
#endif
