#include "compositor/render/ShaderProgram/ShaderProgram.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static void ludash_shader_error(char* error, size_t capacity, const char* message) {
    if (!error || capacity == 0) return;
    snprintf(error, capacity, "%s", message ? message : "unknown shader error");
}

static int ludash_shader_stage_supported(GLenum stage) {
    switch (stage) {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
    case GL_GEOMETRY_SHADER:
    case GL_COMPUTE_SHADER:
    case GL_TESS_CONTROL_SHADER:
    case GL_TESS_EVALUATION_SHADER:
        return 1;
    default:
        return 0;
    }
}

static GLuint ludash_compile_shader(LuDashGLDispatch* gl, GLenum type, const char* source,
                                    char* error, size_t capacity) {
    if (!source || !*source) {
        ludash_shader_error(error, capacity, "shader source is empty");
        return 0;
    }
    const GLuint shader = gl->CreateShader(type);
    if (!shader) {
        ludash_shader_error(error, capacity, "glCreateShader returned 0");
        return 0;
    }
    gl->ShaderSource(shader, 1, &source, NULL);
    gl->CompileShader(shader);
    GLint ok = GL_FALSE;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (error && capacity)
            gl->GetShaderInfoLog(shader,
                                 (GLsizei)(capacity > INT_MAX ? INT_MAX : capacity),
                                 NULL, error);
        gl->DeleteShader(shader);
        return 0;
    }
    return shader;
}

LuDashShaderProgram* ludash_shader_create_stages(
    LuDashGLResolver resolve, const LuDashShaderSource* stages, size_t stage_count,
    char* error, size_t capacity) {
    if (!stages || stage_count == 0) {
        ludash_shader_error(error, capacity, "shader program has no stages");
        return NULL;
    }

    size_t compute_count = 0;
    int has_vertex = 0;
    for (size_t i = 0; i < stage_count; ++i) {
        if (!ludash_shader_stage_supported(stages[i].stage)) {
            ludash_shader_error(error, capacity, "unsupported OpenGL shader stage");
            return NULL;
        }
        if (stages[i].stage == GL_COMPUTE_SHADER) ++compute_count;
        if (stages[i].stage == GL_VERTEX_SHADER) has_vertex = 1;
    }
    if (compute_count && stage_count != 1) {
        ludash_shader_error(error, capacity,
                            "compute shaders cannot be linked with graphics stages");
        return NULL;
    }

    LuDashShaderProgram* shader = calloc(1, sizeof(*shader));
    if (!shader) {
        ludash_shader_error(error, capacity, "could not allocate shader program");
        return NULL;
    }
    if (!ludash_gl_load(&shader->gl, resolve, error, capacity)) {
        free(shader);
        return NULL;
    }

    GLuint* compiled = calloc(stage_count, sizeof(*compiled));
    if (!compiled) {
        ludash_shader_error(error, capacity, "could not allocate shader stage list");
        free(shader);
        return NULL;
    }

    LuDashGLDispatch* gl = &shader->gl;
    for (size_t i = 0; i < stage_count; ++i) {
        compiled[i] = ludash_compile_shader(gl, stages[i].stage, stages[i].source,
                                            error, capacity);
        if (!compiled[i]) {
            for (size_t j = 0; j < i; ++j)
                if (compiled[j]) gl->DeleteShader(compiled[j]);
            free(compiled);
            free(shader);
            return NULL;
        }
    }

    shader->program = gl->CreateProgram();
    if (!shader->program) {
        for (size_t i = 0; i < stage_count; ++i)
            gl->DeleteShader(compiled[i]);
        free(compiled);
        ludash_shader_error(error, capacity, "glCreateProgram returned 0");
        free(shader);
        return NULL;
    }

    for (size_t i = 0; i < stage_count; ++i)
        gl->AttachShader(shader->program, compiled[i]);
    gl->LinkProgram(shader->program);
    for (size_t i = 0; i < stage_count; ++i)
        gl->DeleteShader(compiled[i]);
    free(compiled);

    GLint ok = GL_FALSE;
    gl->GetProgramiv(shader->program, GL_LINK_STATUS, &ok);
    if (!ok) {
        if (error && capacity)
            gl->GetProgramInfoLog(shader->program,
                                  (GLsizei)(capacity > INT_MAX ? INT_MAX : capacity),
                                  NULL, error);
        ludash_shader_destroy(shader);
        return NULL;
    }

    // Raster graphics programs need a VAO for LunaDash's gl_VertexID full-screen
    // passes. Compute-only programs intentionally do not allocate one.
    if (has_vertex) {
        gl->GenVertexArrays(1, &shader->vertex_array);
        if (!shader->vertex_array) {
            ludash_shader_error(error, capacity, "could not create shader vertex array");
            ludash_shader_destroy(shader);
            return NULL;
        }
    }

    return shader;
}

LuDashShaderProgram* ludash_shader_create(LuDashGLResolver resolve,
                                          const char* vertex,
                                          const char* fragment,
                                          char* error,
                                          size_t capacity) {
    const LuDashShaderSource stages[] = {
        {GL_VERTEX_SHADER, vertex},
        {GL_FRAGMENT_SHADER, fragment},
    };
    return ludash_shader_create_stages(resolve, stages, 2, error, capacity);
}

void ludash_shader_destroy(LuDashShaderProgram* shader) {
    if (!shader) return;
    if (shader->vertex_array)
        shader->gl.DeleteVertexArrays(1, &shader->vertex_array);
    if (shader->program)
        shader->gl.DeleteProgram(shader->program);
    free(shader);
}
