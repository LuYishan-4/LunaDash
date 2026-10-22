#include "compositor/renderer/opengl/OpenGL.hpp"

#include <QCoreApplication>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

namespace LunaDash {

OpenGL::OpenGL(GraphicsApi requested, std::shared_ptr<RenderState> state)
    : requested_(requested),
      state_(state ? std::move(state) : std::make_shared<RenderState>()) {}

bool OpenGL::initialize(QString *error) {
  if (initialized_)
    return true;

  context_ = QOpenGLContext::currentContext();
  if (!context_) {
    if (error)
      *error = QStringLiteral("no current OpenGL context");
    state_->failed = true;
    return false;
  }

  const bool es = context_->isOpenGLES();
  const auto format = context_->format();
  state_->isOpenGLES = es;
  state_->majorVersion = format.majorVersion();
  state_->minorVersion = format.minorVersion();

  const bool apiMismatch = (requested_ == GraphicsApi::OpenGL && es) ||
                           (requested_ == GraphicsApi::OpenGLES && !es);
  const bool versionTooOld =
      format.majorVersion() < 3 ||
      (!es && format.majorVersion() == 3 && format.minorVersion() < 3);
  if (apiMismatch || versionTooOld) {
    if (error)
      *error =
          QStringLiteral("requested OpenGL API/context version is unavailable");
    state_->failed = true;
    return false;
  }

  char loadError[1024]{};
  if (!ludash_gl_load(&dispatch_, &OpenGL::resolve, loadError,
                      sizeof(loadError))) {
    if (error)
      *error = QString::fromLocal8Bit(loadError);
    state_->failed = true;
    return false;
  }

  state_->failed = false;
  initialized_ = true;
  return true;
}

void OpenGL::shutdown() {
  // Qt owns the context. GL resources must already have been released by
  // elements.
  initialized_ = false;
  dispatch_ = {};
  context_ = nullptr;
  state_->shaderReady = false;
}

bool OpenGL::initialized() const { return initialized_; }
bool OpenGL::isOpenGLES() const { return state_ && state_->isOpenGLES.load(); }
GraphicsApi OpenGL::requestedApi() const { return requested_; }
LuDashGLDispatch &OpenGL::dispatch() { return dispatch_; }
const LuDashGLDispatch &OpenGL::dispatch() const { return dispatch_; }
std::shared_ptr<RenderState> OpenGL::state() const { return state_; }

LuDashGLProc OpenGL::resolve(const char *name) {
  auto *context = QOpenGLContext::currentContext();
  return context ? context->getProcAddress(name) : nullptr;
}

void OpenGL::configureDefault(GraphicsApi api) {
  const bool es =
      api == GraphicsApi::OpenGLES ||
      (api == GraphicsApi::Auto &&
       QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES);

  QSurfaceFormat format;
  format.setRenderableType(es ? QSurfaceFormat::OpenGLES
                              : QSurfaceFormat::OpenGL);
  format.setVersion(3, es ? 0 : 3);
  format.setProfile(es ? QSurfaceFormat::NoProfile
                       : QSurfaceFormat::CoreProfile);
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  format.setSwapInterval(1);
  QSurfaceFormat::setDefaultFormat(format);
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
}

std::optional<GraphicsApi> graphicsApiFromArguments(int argc, char **argv) {
  QString value = QStringLiteral("auto");
  for (int i = 1; i < argc; ++i) {
    const auto argument = QString::fromLocal8Bit(argv[i]);
    if (argument.startsWith(QStringLiteral("--graphics=")))
      value = argument.mid(11);
    else if (argument == QStringLiteral("--graphics")) {
      if (i + 1 >= argc)
        return std::nullopt;
      value = QString::fromLocal8Bit(argv[++i]);
    }
  }

  if (value == QStringLiteral("opengl"))
    return GraphicsApi::OpenGL;
  if (value == QStringLiteral("gles"))
    return GraphicsApi::OpenGLES;
  if (value == QStringLiteral("auto"))
    return GraphicsApi::Auto;
  return std::nullopt;
}

void configureGraphics(GraphicsApi api) { OpenGL::configureDefault(api); }

} // namespace LunaDash
