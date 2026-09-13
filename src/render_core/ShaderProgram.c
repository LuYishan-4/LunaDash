#include <LuDash/render_core/ShaderProgram.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static GLuint ludash_compile_shader(LuDashGLDispatch* gl, GLenum type, const char* source, char* error, size_t capacity) {
    const GLuint shader = gl->CreateShader(type);
    if (!shader) return 0;
    gl->ShaderSource(shader, 1, &source, NULL); gl->CompileShader(shader);
    GLint ok = GL_FALSE; gl->GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (error && capacity) gl->GetShaderInfoLog(shader, (GLsizei)(capacity > INT_MAX ? INT_MAX : capacity), NULL, error);
        gl->DeleteShader(shader); return 0;
    }
    return shader;
}
LuDashShaderProgram* ludash_shader_create(LuDashGLResolver resolve, const char* vertex, const char* fragment, char* error, size_t capacity) {
    if (!vertex || !fragment) return NULL;
    LuDashShaderProgram* shader = calloc(1, sizeof(*shader));
    if (!shader) return NULL;
    if (!ludash_gl_load(&shader->gl, resolve, error, capacity)) { free(shader); return NULL; }
    LuDashGLDispatch* gl = &shader->gl;
    const GLuint vert = ludash_compile_shader(gl, GL_VERTEX_SHADER, vertex, error, capacity);
    if (!vert) { free(shader); return NULL; }
    const GLuint frag = ludash_compile_shader(gl, GL_FRAGMENT_SHADER, fragment, error, capacity);
    if (!frag) { gl->DeleteShader(vert); free(shader); return NULL; }
    shader->program = gl->CreateProgram();
    if (!shader->program) { gl->DeleteShader(vert); gl->DeleteShader(frag); free(shader); return NULL; }
    gl->AttachShader(shader->program, vert); gl->AttachShader(shader->program, frag); gl->LinkProgram(shader->program);
    gl->DeleteShader(vert); gl->DeleteShader(frag);
    GLint ok = GL_FALSE; gl->GetProgramiv(shader->program, GL_LINK_STATUS, &ok);
    if (!ok) {
        if (error && capacity) gl->GetProgramInfoLog(shader->program, (GLsizei)(capacity > INT_MAX ? INT_MAX : capacity), NULL, error);
        ludash_shader_destroy(shader); return NULL;
    }
    gl->GenVertexArrays(1, &shader->vertex_array);
    if (!shader->vertex_array) { ludash_shader_destroy(shader); return NULL; }
    return shader;
}
void ludash_shader_destroy(LuDashShaderProgram* shader) {
    if (!shader) return;
    if (shader->vertex_array) shader->gl.DeleteVertexArrays(1, &shader->vertex_array);
    if (shader->program) shader->gl.DeleteProgram(shader->program);
    free(shader);
}
