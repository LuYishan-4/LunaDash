#include "compositor/renderer/Renderer.hpp"
#include "compositor/renderer/opengl/Framebuffer.hpp"
#include "compositor/renderer/opengl/Program.hpp"
#include "compositor/renderer/opengl/ShaderAsset.hpp"
#include "compositor/renderer/opengl/wallpaper/WallpaperElement.hpp"
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <cstring>
#include <set>

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

bool check(bool ok, const QString &message) {
  if (!ok)
    qCritical().noquote() << message;
  return ok;
}

bool resourceTests() {
  // Deliberately leave the build/source/install directory before loading
  // assets.
  const auto previousDirectory = QDir::currentPath();
  QTemporaryDir directory;
  const auto restoreDirectory =
      qScopeGuard([&] { QDir::setCurrent(previousDirectory); });
  if (!directory.isValid() || !QDir::setCurrent(directory.path()))
    return false;
  for (const auto &name :
       {"Fullscreen.vert", "Wallpaper.frag", "Blur.frag", "Decoration.frag"}) {
    for (bool es : {false, true}) {
      QString error;
      const auto asset =
          ShaderAssetLoader::load(QString::fromLatin1(name), es, &error);
      if (!check(asset.has_value(), error))
        return false;
      if (!check(asset->source.startsWith(es ? "#version 300 es"
                                             : "#version 330 core"),
                 "Missing shader version"))
        return false;
    }
  }
  QString error;
  if (!check(!ShaderAssetLoader::load("Missing.frag", false, &error) &&
                 error.contains("Missing.frag"),
             "Missing asset has no diagnostic"))
    return false;
  Renderer renderer;
  if (!check(!renderer.initialize(&error) &&
                 error.contains("no current OpenGL context"),
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
  const QList<ShaderAsset> valid{{"Vertex", ShaderStage::Vertex, "source"},
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
    QString error;
    auto program = Program::create(gl, assets, &error);
    if (!check(bool(program) == (scenario == 0),
               QString("Unexpected result for shader failure scenario %1")
                   .arg(scenario)))
      return false;
    program.reset();
    if (!check(fake.shaders.empty() && fake.programs.empty() &&
                   fake.arrays.empty(),
               QString("GL resource leak in scenario %1").arg(scenario)))
      return false;
  }
  return true;
}

bool openGLTests() {
  QOpenGLContext context;
  context.setFormat(QSurfaceFormat::defaultFormat());
  if (!check(context.create(), "Could not create software OpenGL test context"))
    return false;
  QOffscreenSurface surface;
  surface.setFormat(context.format());
  surface.create();
  if (!check(context.makeCurrent(&surface),
             "Could not make test context current"))
    return false;
  {
    Renderer renderer;
    QString error;
    if (!check(renderer.initialize(&error), error))
      return false;
    // Compile every shipped fragment against the fullscreen vertex shader.
    for (const auto &fragment :
         {"Wallpaper.frag", "Blur.frag", "Decoration.frag"}) {
      auto program = renderer.createProgram(
          {"Fullscreen.vert", QString::fromLatin1(fragment)}, &error);
      if (!check(bool(program), error))
        return false;
    }
    Framebuffer target;
    if (!check(target.resize(renderer.context().dispatch(), {32, 32}),
               "Framebuffer creation failed"))
      return false;
    target.bind();
    WallpaperElement wallpaper;
    if (!check(renderer.render(wallpaper, {{0, 0, 32, 32}, 1.0f}, &error),
               error))
      return false;
    wallpaper.release();
    target.reset();
  }
  context.doneCurrent();
  return true;
}
} // namespace

int run(int argc, char **argv) {
  const bool gpu = argc > 1 && std::strcmp(argv[1], "--opengl") == 0;
  if (gpu)
    configureGraphics(GraphicsApi::OpenGL);
  QGuiApplication app(argc, argv);
  if (!resourceTests() || !lifetimeTests() || (gpu && !openGLTests()))
    return 1;
  qInfo("Embedded shaders, context diagnostics and GL resource lifetimes "
        "passed.");
  return 0;
}
} // namespace LunaDash::Tests

int main(int argc, char **argv) { return LunaDash::Tests::run(argc, argv); }
