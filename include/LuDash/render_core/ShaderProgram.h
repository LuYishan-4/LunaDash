#pragma once
#include <LuDash/render_core/GLDispatch.h>
#ifdef __cplusplus
namespace LuDash {
extern "C" {
#endif
typedef struct LuDashShaderProgram {
    LuDashGLDispatch gl;
    GLuint program;
    GLuint vertex_array;
} LuDashShaderProgram;
LuDashShaderProgram* ludash_shader_create(LuDashGLResolver resolve, const char* vertex, const char* fragment, char* error, size_t capacity);
void ludash_shader_destroy(LuDashShaderProgram* shader);
int ludash_wallpaper_draw(LuDashShaderProgram* shader, int width, int height, int palette);
#ifdef __cplusplus
}
}
#endif
