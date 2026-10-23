#include "compositor/renderer/Renderer.hpp"
#include "compositor/renderer/opengl/Framebuffer.hpp"
#include "compositor/renderer/opengl/Program.hpp"
#include "compositor/renderer/opengl/ShaderAsset.hpp"
#include "compositor/renderer/opengl/wallpaper/WallpaperElement.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cstring>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

namespace LunaDash::Tests {
namespace {
struct FakeGL {
  GLuint next = 1;
  int shaderCalls = 0;
  int failCreate = 0;
  int failCompile = 0;
  bool failProgram = false;
  bool failLink = false;
  bool failVertexArray = false;
  std::set<GLuint> shaders;
  std::set<GLuint> programs;
  std::set<GLuint> arrays;
} fake;

GLuint createShader(GLenum) {
  if (++fake.shaderCalls == fake.failCreate)
    return 0;
  const auto id = fake.next++;
  fake.shaders.insert(id);
  return id;
}
void shaderSource(GLuint, GLsizei, const GLchar *const *, const GLint *) {}
void compileShader(GLuint) {}
void shaderStatus(GLuint, GLenum property, GLint *value) {
  *value =
      property == GL_COMPILE_STATUS ? fake.shaderCalls != fake.failCompile : 1;
}
void emptyLog(GLuint, GLsizei, GLsizei *length, GLchar *text) {
  *length = 0;
  *text = '\0';
}
void deleteShader(GLuint id) { fake.shaders.erase(id); }
GLuint createProgram() {
  if (fake.failProgram)
    return 0;
  const auto id = fake.next++;
  fake.programs.insert(id);
  return id;
}
void attachShader(GLuint, GLuint) {}
void linkProgram(GLuint) {}
void programStatus(GLuint, GLenum property, GLint *value) {
  *value = property == GL_LINK_STATUS ? !fake.failLink : 1;
}
void deleteProgram(GLuint id) { fake.programs.erase(id); }
void createArrays(GLsizei count, GLuint *ids) {
  for (int i = 0; i < count; ++i) {
    ids[i] = fake.failVertexArray ? 0 : fake.next++;
    if (ids[i])
      fake.arrays.insert(ids[i]);
  }
}
void deleteArrays(GLsizei count, const GLuint *ids) {
  for (int i = 0; i < count; ++i)
    fake.arrays.erase(ids[i]);
}

bool check(bool condition, const std::string &message) {
  if (!condition)
    std::cerr << message << '\n';
  return condition;
}

class TemporaryWorkingDirectory final {
public:
  TemporaryWorkingDirectory() : previous_(std::filesystem::current_path()) {
    const auto base = std::filesystem::temp_directory_path();
    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
      path_ =
          base / ("lunadash-renderer-test-" +
                  std::to_string(static_cast<unsigned long long>(::getpid())) +
                  "-" + std::to_string(attempt));
      std::error_code error;
      if (!std::filesystem::create_directory(path_, error))
        continue;
      std::filesystem::current_path(path_);
      valid_ = true;
      return;
    }
  }

  ~TemporaryWorkingDirectory() {
    std::error_code error;
    std::filesystem::current_path(previous_, error);
    if (valid_)
      std::filesystem::remove_all(path_, error);
  }

  bool valid() const { return valid_; }

private:
  std::filesystem::path previous_;
  std::filesystem::path path_;
  bool valid_ = false;
};

class HeadlessEglContext final {
public:
  ~HeadlessEglContext() {
    if (display_ != EGL_NO_DISPLAY) {
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (context_ != EGL_NO_CONTEXT)
        eglDestroyContext(display_, context_);
      if (surface_ != EGL_NO_SURFACE)
        eglDestroySurface(display_, surface_);
      eglTerminate(display_);
    }
  }

  bool initialize(std::string *error) {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY)
      return fail(error, "EGL could not open the default display");
    EGLint major = 0;
    EGLint minor = 0;
    if (eglInitialize(display_, &major, &minor) != EGL_TRUE)
      return fail(error, "EGL initialization failed");
    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE)
      return fail(error, "EGL could not bind the OpenGL API");

    const EGLint configAttributes[] = {EGL_SURFACE_TYPE,
                                       EGL_PBUFFER_BIT,
                                       EGL_RENDERABLE_TYPE,
                                       EGL_OPENGL_BIT,
                                       EGL_RED_SIZE,
                                       8,
                                       EGL_GREEN_SIZE,
                                       8,
                                       EGL_BLUE_SIZE,
                                       8,
                                       EGL_NONE};
    EGLConfig config = nullptr;
    EGLint configCount = 0;
    if (eglChooseConfig(display_, configAttributes, &config, 1, &configCount) !=
            EGL_TRUE ||
        configCount != 1)
      return fail(error, "EGL has no OpenGL pbuffer configuration");

    const EGLint surfaceAttributes[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32,
                                        EGL_NONE};
    surface_ = eglCreatePbufferSurface(display_, config, surfaceAttributes);
    if (surface_ == EGL_NO_SURFACE)
      return fail(error, "EGL could not create a pbuffer surface");

    const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION_KHR,
                                        3,
                                        EGL_CONTEXT_MINOR_VERSION_KHR,
                                        3,
                                        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                        EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                                        EGL_NONE};
    context_ =
        eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttributes);
    if (context_ == EGL_NO_CONTEXT)
      return fail(error, "EGL could not create an OpenGL 3.3 context");
    if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE)
      return fail(error, "EGL could not make the pbuffer context current");
    return true;
  }

private:
  static bool fail(std::string *error, const char *message) {
    if (error)
      *error = message;
    return false;
  }

  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLSurface surface_ = EGL_NO_SURFACE;
  EGLContext context_ = EGL_NO_CONTEXT;
};

bool resourceTests() {
  TemporaryWorkingDirectory directory;
  if (!check(directory.valid(),
             "Could not create a temporary working directory"))
    return false;
  for (const auto *name :
       {"Fullscreen.vert", "Wallpaper.frag", "Blur.frag", "Decoration.frag"}) {
    for (bool es : {false, true}) {
      std::string error;
      const auto asset = ShaderAssetLoader::load(name, es, &error);
      if (!check(asset.has_value(), error))
        return false;
      if (!check(asset->source.starts_with(es ? "#version 300 es"
                                              : "#version 330 core"),
                 "Missing embedded shader version"))
        return false;
    }
  }
  std::string error;
  if (!check(!ShaderAssetLoader::load("Missing.frag", false, &error) &&
                 error.find("Missing.frag") != std::string::npos,
             "Missing asset has no diagnostic"))
    return false;
  Renderer renderer;
  if (!check(!renderer.initialize(&error) &&
                 error.find("no current EGL/OpenGL context") !=
                     std::string::npos,
             "Missing context must fail with a useful error"))
    return false;
  return true;
}

bool lifetimeTests() {
  LuDashGLDispatch gl{};
  gl.CreateShader = createShader;
  gl.ShaderSource = shaderSource;
  gl.CompileShader = compileShader;
  gl.GetShaderiv = shaderStatus;
  gl.GetShaderInfoLog = emptyLog;
  gl.DeleteShader = deleteShader;
  gl.CreateProgram = createProgram;
  gl.AttachShader = attachShader;
  gl.LinkProgram = linkProgram;
  gl.GetProgramiv = programStatus;
  gl.GetProgramInfoLog = emptyLog;
  gl.DeleteProgram = deleteProgram;
  gl.GenVertexArrays = createArrays;
  gl.DeleteVertexArrays = deleteArrays;
  const std::vector<ShaderAsset> valid{
      {"Vertex", ShaderStage::Vertex, "source"},
      {"Fragment", ShaderStage::Fragment, "source"}};
  for (int scenario = 0; scenario < 9; ++scenario) {
    fake = {};
    auto assets = valid;
    if (scenario == 1)
      assets[1].stage = ShaderStage::Unknown;
    if (scenario == 2)
      fake.failCreate = 2;
    if (scenario == 3)
      fake.failCompile = 2;
    if (scenario == 4)
      assets[1].stage = ShaderStage::Compute;
    if (scenario == 5)
      fake.failProgram = true;
    if (scenario == 6)
      fake.failLink = true;
    if (scenario == 7)
      fake.failVertexArray = true;
    if (scenario == 8)
      assets.clear();
    std::string error;
    auto program = Program::create(gl, assets, &error);
    if (!check(static_cast<bool>(program) == (scenario == 0),
               "Unexpected shader failure scenario " +
                   std::to_string(scenario)))
      return false;
    program.reset();
    if (!check(fake.shaders.empty() && fake.programs.empty() &&
                   fake.arrays.empty(),
               "GL resource leak in scenario " + std::to_string(scenario)))
      return false;
  }
  return true;
}

bool openGLTests() {
  HeadlessEglContext context;
  std::string error;
  const bool contextReady = context.initialize(&error);
  if (!check(contextReady, error))
    return false;
  {
    Renderer renderer;
    if (!check(renderer.initialize(&error), error))
      return false;
    for (const auto *fragment :
         {"Wallpaper.frag", "Blur.frag", "Decoration.frag"}) {
      auto program =
          renderer.createProgram({"Fullscreen.vert", fragment}, &error);
      if (!check(static_cast<bool>(program), error))
        return false;
    }
    Framebuffer target;
    if (!check(target.resize(renderer.context().dispatch(), {32, 32}),
               "Framebuffer creation failed"))
      return false;
    target.bind();
    WallpaperElement wallpaper;
    const ElementRenderContext renderContext{{0, 0, 32, 32}, 1.0f};
    if (!check(renderer.render(wallpaper, renderContext, &error), error))
      return false;
    wallpaper.release();
    target.reset();
  }
  return true;
}
} // namespace

int run(int argc, char **argv) {
  const bool gpu = argc > 1 && std::strcmp(argv[1], "--opengl") == 0;
  if (!resourceTests() || !lifetimeTests() || (gpu && !openGLTests()))
    return 1;
  std::cout << "Embedded shaders, EGL context handling and GL resource "
               "lifetimes passed.\n";
  return 0;
}
} // namespace LunaDash::Tests

int main(int argc, char **argv) { return LunaDash::Tests::run(argc, argv); }
