#include <LuDash/render_core/BlurPass.h>
#include <math.h>
#include <stdlib.h>
struct LuDashBlurPass {
    LuDashShaderProgram* shader;
    GLuint textures[2];
    GLuint framebuffers[2];
    int width, height;
};
LuDashBlurPass* ludash_blur_create(LuDashGLResolver resolve, const char* vertex, const char* fragment, char* error, size_t capacity) {
    LuDashBlurPass* pass = calloc(1, sizeof(*pass));
    if (!pass) return NULL;
    pass->shader = ludash_shader_create(resolve, vertex, fragment, error, capacity);
    if (!pass->shader) { free(pass); return NULL; }
    return pass;
}
void ludash_blur_destroy(LuDashBlurPass* pass) {
    if (!pass) return;
    LuDashGLDispatch* gl = &pass->shader->gl;
    gl->DeleteTextures(2, pass->textures); gl->DeleteFramebuffers(2, pass->framebuffers);
    ludash_shader_destroy(pass->shader); free(pass);
}
static int ludash_blur_resize(LuDashBlurPass* pass, int width, int height) {
    if (pass->width == width && pass->height == height) return 1;
    LuDashGLDispatch* gl = &pass->shader->gl;
    gl->DeleteTextures(2, pass->textures); gl->DeleteFramebuffers(2, pass->framebuffers);
    gl->GenTextures(2, pass->textures); gl->GenFramebuffers(2, pass->framebuffers);
    for (int i = 0; i < 2; ++i) {
        if (!pass->textures[i] || !pass->framebuffers[i]) { pass->width = 0; pass->height = 0; return 0; }
        gl->BindTexture(GL_TEXTURE_2D, pass->textures[i]);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        gl->BindFramebuffer(GL_FRAMEBUFFER, pass->framebuffers[i]);
        gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pass->textures[i], 0);
        if (gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { pass->width = 0; pass->height = 0; return 0; }
    }
    pass->width = width; pass->height = height; return 1;
}
int ludash_blur_draw(LuDashBlurPass* pass, const LuDashBlurRegion* region) {
    if (!pass || !region || region->width <= 0 || region->height <= 0 || region->width > 32768 || region->height > 32768 ||
        region->x < 0 || region->y < 0 || region->x > 32768 || region->y > 32768 || region->radius < 0 || region->radius > 32 ||
        !isfinite(region->opacity) || region->opacity < 0 || region->opacity > 1) return 0;
    LuDashShaderProgram* shader = pass->shader;
    LuDashGLDispatch* gl = &shader->gl;
    GLint viewport[4], target = 0, read_target = 0;
    gl->GetIntegerv(GL_VIEWPORT, viewport);
    gl->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target); gl->GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_target);
    const int width = region->width > 1 ? region->width / 2 : 1;
    const int height = region->height > 1 ? region->height / 2 : 1;
    gl->ActiveTexture(GL_TEXTURE0);
    if (!ludash_blur_resize(pass, width, height)) {
        gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)target); gl->BindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_target); return 0;
    }
    gl->Disable(GL_SCISSOR_TEST); gl->Disable(GL_STENCIL_TEST); gl->Disable(GL_DEPTH_TEST); gl->Disable(GL_CULL_FACE); gl->Disable(GL_BLEND);
    gl->DepthMask(GL_FALSE); gl->ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl->BindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)target); gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, pass->framebuffers[0]);
    gl->BlitFramebuffer(region->x, region->y, region->x + region->width, region->y + region->height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    gl->BindFramebuffer(GL_FRAMEBUFFER, pass->framebuffers[1]); gl->Viewport(0, 0, width, height);
    gl->BindTexture(GL_TEXTURE_2D, pass->textures[0]);
    gl->UseProgram(shader->program); gl->Uniform1i(gl->GetUniformLocation(shader->program, "sourceTexture"), 0);
    gl->Uniform1f(gl->GetUniformLocation(shader->program, "opacity"), 1.0f);
    gl->Uniform1f(gl->GetUniformLocation(shader->program, "radius"), (float)region->radius * 0.5f);
    gl->Uniform2f(gl->GetUniformLocation(shader->program, "direction"), 1.0f / (float)width, 0.0f);
    gl->BindVertexArray(shader->vertex_array); gl->DrawArrays(GL_TRIANGLES, 0, 3);
    gl->BindFramebuffer(GL_FRAMEBUFFER, (GLuint)target); gl->Viewport(region->x, region->y, region->width, region->height);
    gl->BindTexture(GL_TEXTURE_2D, pass->textures[1]);
    gl->Uniform2f(gl->GetUniformLocation(shader->program, "direction"), 0.0f, 1.0f / (float)height);
    gl->Uniform1f(gl->GetUniformLocation(shader->program, "opacity"), region->opacity);
    gl->Enable(GL_BLEND); gl->BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (region->scissor_enabled) { gl->Enable(GL_SCISSOR_TEST); gl->Scissor(region->scissor[0], region->scissor[1], region->scissor[2], region->scissor[3]); }
    if (region->stencil_enabled) { gl->Enable(GL_STENCIL_TEST); gl->StencilFunc(GL_EQUAL, region->stencil_value, 0xff); gl->StencilOp(GL_KEEP, GL_KEEP, GL_KEEP); }
    gl->DrawArrays(GL_TRIANGLES, 0, 3); gl->BindVertexArray(0); gl->UseProgram(0);
    gl->BindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_target); gl->Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    return 1;
}
