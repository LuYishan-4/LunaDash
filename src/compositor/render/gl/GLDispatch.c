#include "compositor/render/gl/GLDispatch.h"
static void ludash_gl_error(char* output, size_t capacity, const char* message) {
    if (!output || !capacity) return;
    size_t index = 0;
    while (index < capacity - 1 && message[index]) { output[index] = message[index]; ++index; }
    output[index] = '\0';
}
int ludash_gl_load(LuDashGLDispatch* gl, LuDashGLResolver resolve, char* error, size_t capacity) {
    if (!gl || !resolve) return 0;
    gl->CreateShader = (PFNGLCREATESHADERPROC)resolve("glCreateShader");
    if (!gl->CreateShader) { ludash_gl_error(error, capacity, "Missing OpenGL function: glCreateShader"); return 0; }
    gl->ShaderSource = (PFNGLSHADERSOURCEPROC)resolve("glShaderSource");
    if (!gl->ShaderSource) { ludash_gl_error(error, capacity, "Missing OpenGL function: glShaderSource"); return 0; }
    gl->CompileShader = (PFNGLCOMPILESHADERPROC)resolve("glCompileShader");
    if (!gl->CompileShader) { ludash_gl_error(error, capacity, "Missing OpenGL function: glCompileShader"); return 0; }
    gl->GetShaderiv = (PFNGLGETSHADERIVPROC)resolve("glGetShaderiv");
    if (!gl->GetShaderiv) { ludash_gl_error(error, capacity, "Missing OpenGL function: glGetShaderiv"); return 0; }
    gl->GetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)resolve("glGetShaderInfoLog");
    if (!gl->GetShaderInfoLog) { ludash_gl_error(error, capacity, "Missing OpenGL function: glGetShaderInfoLog"); return 0; }
    gl->DeleteShader = (PFNGLDELETESHADERPROC)resolve("glDeleteShader");
    if (!gl->DeleteShader) { ludash_gl_error(error, capacity, "Missing OpenGL function: glDeleteShader"); return 0; }
    gl->CreateProgram = (PFNGLCREATEPROGRAMPROC)resolve("glCreateProgram");
    if (!gl->CreateProgram) { ludash_gl_error(error, capacity, "Missing OpenGL function: glCreateProgram"); return 0; }
    gl->AttachShader = (PFNGLATTACHSHADERPROC)resolve("glAttachShader");
    if (!gl->AttachShader) { ludash_gl_error(error, capacity, "Missing OpenGL function: glAttachShader"); return 0; }
    gl->LinkProgram = (PFNGLLINKPROGRAMPROC)resolve("glLinkProgram");
    if (!gl->LinkProgram) { ludash_gl_error(error, capacity, "Missing OpenGL function: glLinkProgram"); return 0; }
    gl->GetProgramiv = (PFNGLGETPROGRAMIVPROC)resolve("glGetProgramiv");
    if (!gl->GetProgramiv) { ludash_gl_error(error, capacity, "Missing OpenGL function: glGetProgramiv"); return 0; }
    gl->GetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)resolve("glGetProgramInfoLog");
    if (!gl->GetProgramInfoLog) { ludash_gl_error(error, capacity, "Missing OpenGL function: glGetProgramInfoLog"); return 0; }
    gl->DeleteProgram = (PFNGLDELETEPROGRAMPROC)resolve("glDeleteProgram");
    if (!gl->DeleteProgram) { ludash_gl_error(error, capacity, "Missing OpenGL function: glDeleteProgram"); return 0; }
    gl->UseProgram = (PFNGLUSEPROGRAMPROC)resolve("glUseProgram");
    if (!gl->UseProgram) { ludash_gl_error(error, capacity, "Missing OpenGL function: glUseProgram"); return 0; }
    gl->GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)resolve("glGetUniformLocation");
    if (!gl->GetUniformLocation) { ludash_gl_error(error, capacity, "Missing OpenGL function: glGetUniformLocation"); return 0; }
    gl->Uniform1i = (PFNGLUNIFORM1IPROC)resolve("glUniform1i");
    if (!gl->Uniform1i) { ludash_gl_error(error, capacity, "Missing OpenGL function: glUniform1i"); return 0; }
    gl->Uniform1f = (PFNGLUNIFORM1FPROC)resolve("glUniform1f");
    if (!gl->Uniform1f) { ludash_gl_error(error, capacity, "Missing OpenGL function: glUniform1f"); return 0; }
    gl->Uniform1fv = (PFNGLUNIFORM1FVPROC)resolve("glUniform1fv");
    if (!gl->Uniform1fv) { ludash_gl_error(error, capacity, "Missing OpenGL function: glUniform1fv"); return 0; }
    gl->Uniform2f = (PFNGLUNIFORM2FPROC)resolve("glUniform2f");
    if (!gl->Uniform2f) { ludash_gl_error(error, capacity, "Missing OpenGL function: glUniform2f"); return 0; }
    gl->GenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)resolve("glGenVertexArrays");
    if (!gl->GenVertexArrays) { ludash_gl_error(error, capacity, "Missing OpenGL function: glGenVertexArrays"); return 0; }
    gl->DeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)resolve("glDeleteVertexArrays");
    if (!gl->DeleteVertexArrays) { ludash_gl_error(error, capacity, "Missing OpenGL function: glDeleteVertexArrays"); return 0; }
    gl->BindVertexArray = (PFNGLBINDVERTEXARRAYPROC)resolve("glBindVertexArray");
    if (!gl->BindVertexArray) { ludash_gl_error(error, capacity, "Missing OpenGL function: glBindVertexArray"); return 0; }
    gl->DrawArrays = (PFNGLDRAWARRAYSPROC)resolve("glDrawArrays");
    if (!gl->DrawArrays) { ludash_gl_error(error, capacity, "Missing OpenGL function: glDrawArrays"); return 0; }
    gl->GetIntegerv = (PFNGLGETINTEGERVPROC)resolve("glGetIntegerv");
    if (!gl->GetIntegerv) { ludash_gl_error(error, capacity, "Missing OpenGL function: glGetIntegerv"); return 0; }
    gl->Viewport = (PFNGLVIEWPORTPROC)resolve("glViewport");
    if (!gl->Viewport) { ludash_gl_error(error, capacity, "Missing OpenGL function: glViewport"); return 0; }
    gl->Enable = (PFNGLENABLEPROC)resolve("glEnable");
    if (!gl->Enable) { ludash_gl_error(error, capacity, "Missing OpenGL function: glEnable"); return 0; }
    gl->Disable = (PFNGLDISABLEPROC)resolve("glDisable");
    if (!gl->Disable) { ludash_gl_error(error, capacity, "Missing OpenGL function: glDisable"); return 0; }
    gl->DepthMask = (PFNGLDEPTHMASKPROC)resolve("glDepthMask");
    if (!gl->DepthMask) { ludash_gl_error(error, capacity, "Missing OpenGL function: glDepthMask"); return 0; }
    gl->ColorMask = (PFNGLCOLORMASKPROC)resolve("glColorMask");
    if (!gl->ColorMask) { ludash_gl_error(error, capacity, "Missing OpenGL function: glColorMask"); return 0; }
    gl->BlendFunc = (PFNGLBLENDFUNCPROC)resolve("glBlendFunc");
    if (!gl->BlendFunc) { ludash_gl_error(error, capacity, "Missing OpenGL function: glBlendFunc"); return 0; }
    gl->Scissor = (PFNGLSCISSORPROC)resolve("glScissor");
    if (!gl->Scissor) { ludash_gl_error(error, capacity, "Missing OpenGL function: glScissor"); return 0; }
    gl->StencilFunc = (PFNGLSTENCILFUNCPROC)resolve("glStencilFunc");
    if (!gl->StencilFunc) { ludash_gl_error(error, capacity, "Missing OpenGL function: glStencilFunc"); return 0; }
    gl->StencilOp = (PFNGLSTENCILOPPROC)resolve("glStencilOp");
    if (!gl->StencilOp) { ludash_gl_error(error, capacity, "Missing OpenGL function: glStencilOp"); return 0; }
    gl->GenTextures = (PFNGLGENTEXTURESPROC)resolve("glGenTextures");
    if (!gl->GenTextures) { ludash_gl_error(error, capacity, "Missing OpenGL function: glGenTextures"); return 0; }
    gl->DeleteTextures = (PFNGLDELETETEXTURESPROC)resolve("glDeleteTextures");
    if (!gl->DeleteTextures) { ludash_gl_error(error, capacity, "Missing OpenGL function: glDeleteTextures"); return 0; }
    gl->ActiveTexture = (PFNGLACTIVETEXTUREPROC)resolve("glActiveTexture");
    if (!gl->ActiveTexture) { ludash_gl_error(error, capacity, "Missing OpenGL function: glActiveTexture"); return 0; }
    gl->BindTexture = (PFNGLBINDTEXTUREPROC)resolve("glBindTexture");
    if (!gl->BindTexture) { ludash_gl_error(error, capacity, "Missing OpenGL function: glBindTexture"); return 0; }
    gl->TexParameteri = (PFNGLTEXPARAMETERIPROC)resolve("glTexParameteri");
    if (!gl->TexParameteri) { ludash_gl_error(error, capacity, "Missing OpenGL function: glTexParameteri"); return 0; }
    gl->TexImage2D = (PFNGLTEXIMAGE2DPROC)resolve("glTexImage2D");
    if (!gl->TexImage2D) { ludash_gl_error(error, capacity, "Missing OpenGL function: glTexImage2D"); return 0; }
    gl->GenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)resolve("glGenFramebuffers");
    if (!gl->GenFramebuffers) { ludash_gl_error(error, capacity, "Missing OpenGL function: glGenFramebuffers"); return 0; }
    gl->DeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)resolve("glDeleteFramebuffers");
    if (!gl->DeleteFramebuffers) { ludash_gl_error(error, capacity, "Missing OpenGL function: glDeleteFramebuffers"); return 0; }
    gl->BindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)resolve("glBindFramebuffer");
    if (!gl->BindFramebuffer) { ludash_gl_error(error, capacity, "Missing OpenGL function: glBindFramebuffer"); return 0; }
    gl->FramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)resolve("glFramebufferTexture2D");
    if (!gl->FramebufferTexture2D) { ludash_gl_error(error, capacity, "Missing OpenGL function: glFramebufferTexture2D"); return 0; }
    gl->CheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)resolve("glCheckFramebufferStatus");
    if (!gl->CheckFramebufferStatus) { ludash_gl_error(error, capacity, "Missing OpenGL function: glCheckFramebufferStatus"); return 0; }
    gl->BlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)resolve("glBlitFramebuffer");
    if (!gl->BlitFramebuffer) { ludash_gl_error(error, capacity, "Missing OpenGL function: glBlitFramebuffer"); return 0; }
    return 1;
}
