#include "compositor/renderer/opengl/OpenGL.hpp"

#include <EGL/egl.h>

#include <algorithm>
#include <string_view>
#include <utility>

namespace LunaDash {

OpenGL::OpenGL(GraphicsApi requested, std::shared_ptr<RenderState> state)
    : requested_(requested),
      state_(state ? std::move(state) : std::make_shared<RenderState>()) {}

bool OpenGL::initialize(std::string *error) {
  if (initialized_)
    return true;
  if (error)
    error->clear();

  const EGLContext context = eglGetCurrentContext();
  const EGLDisplay display = eglGetCurrentDisplay();
  if (context == EGL_NO_CONTEXT || display == EGL_NO_DISPLAY) {
    if (error)
      *error = "no current EGL/OpenGL context";
    state_->failed = true;
    return false;
  }

  EGLint clientType = EGL_NONE;
  if (eglQueryContext(display, context, EGL_CONTEXT_CLIENT_TYPE, &clientType) !=
      EGL_TRUE) {
    if (error)
      *error = "could not query the current EGL context API";
    state_->failed = true;
    return false;
  }
  const bool es = clientType == EGL_OPENGL_ES_API;
  if (!es && clientType != EGL_OPENGL_API) {
    if (error)
      *error = "the current EGL context is not an OpenGL context";
    state_->failed = true;
    return false;
  }

  const bool apiMismatch = (requested_ == GraphicsApi::OpenGL && es) ||
                           (requested_ == GraphicsApi::OpenGLES && !es);
  if (apiMismatch) {
    if (error)
      *error = "requested OpenGL API does not match the current EGL context";
    state_->failed = true;
    return false;
  }

  char loadError[1024]{};
  if (!ludash_gl_load(&dispatch_, &OpenGL::resolve, loadError,
                      sizeof(loadError))) {
    if (error)
      *error = loadError;
    state_->failed = true;
    return false;
  }

  GLint major = 0;
  GLint minor = 0;
  dispatch_.GetIntegerv(GL_MAJOR_VERSION, &major);
  dispatch_.GetIntegerv(GL_MINOR_VERSION, &minor);
  const bool versionTooOld = major < 3 || (!es && major == 3 && minor < 3);
  if (versionTooOld) {
    if (error)
      *error = es ? "OpenGL ES 3.0 or newer is required"
                  : "OpenGL 3.3 or newer is required";
    dispatch_ = {};
    state_->failed = true;
    return false;
  }

  state_->isOpenGLES = es;
  state_->majorVersion = major;
  state_->minorVersion = minor;
  state_->failed = false;
  initialized_ = true;
  return true;
}

void OpenGL::shutdown() {
  // EGL and the toolkit own the current context. LunaDash only owns the GL
  // dispatch table and must release its resources before that context ends.
  initialized_ = false;
  dispatch_ = {};
  state_->shaderReady = false;
}

bool OpenGL::initialized() const { return initialized_; }
bool OpenGL::isOpenGLES() const { return state_ && state_->isOpenGLES.load(); }
GraphicsApi OpenGL::requestedApi() const { return requested_; }
LuDashGLDispatch &OpenGL::dispatch() { return dispatch_; }
const LuDashGLDispatch &OpenGL::dispatch() const { return dispatch_; }
std::shared_ptr<RenderState> OpenGL::state() const { return state_; }

LuDashGLProc OpenGL::resolve(const char *name) {
  return reinterpret_cast<LuDashGLProc>(eglGetProcAddress(name));
}

std::optional<GraphicsApi> graphicsApiFromArguments(int argc, char **argv) {
  std::string_view value = "auto";
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument.starts_with("--graphics=")) {
      value = argument.substr(11);
      break;
    }
    if (argument == "--graphics") {
      if (index + 1 >= argc)
        return std::nullopt;
      value = argv[index + 1];
      break;
    }
  }

  if (value == "opengl")
    return GraphicsApi::OpenGL;
  if (value == "gles")
    return GraphicsApi::OpenGLES;
  if (value == "auto")
    return GraphicsApi::Auto;
  return std::nullopt;
}

} // namespace LunaDash
