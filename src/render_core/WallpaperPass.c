#include <LuDash/render_core/ShaderProgram.h>
int ludash_wallpaper_draw(LuDashShaderProgram* shader, int width, int height, int palette) {
    if (!shader || width <= 0 || height <= 0 || width > 32768 || height > 32768) return 0;
    LuDashGLDispatch* gl = &shader->gl;
    gl->Viewport(0, 0, width, height);
    gl->Disable(GL_DEPTH_TEST); gl->Disable(GL_SCISSOR_TEST); gl->Disable(GL_BLEND);
    gl->UseProgram(shader->program);
    gl->Uniform2f(gl->GetUniformLocation(shader->program, "resolution"), (float)width, (float)height);
    gl->Uniform1i(gl->GetUniformLocation(shader->program, "palette"), palette);
    gl->BindVertexArray(shader->vertex_array); gl->DrawArrays(GL_TRIANGLES, 0, 3); gl->BindVertexArray(0);
    gl->UseProgram(0); return 1;
}
