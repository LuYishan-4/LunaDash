#include "compositor/render/shader/ShaderProgram.hpp"

#include <QByteArray>
#include <QVector>
#include <algorithm>
#include <limits>

namespace LuDash {
namespace {

bool supported(GLenum stage) {
  switch (stage) {
  case GL_VERTEX_SHADER:
  case GL_FRAGMENT_SHADER:
  case GL_GEOMETRY_SHADER:
  case GL_COMPUTE_SHADER:
  case GL_TESS_CONTROL_SHADER:
  case GL_TESS_EVALUATION_SHADER:
    return true;
  default:
    return false;
  }
}

QString shaderLog(LuDashGLDispatch &gl, GLuint shader) {
  GLint length = 0;
  gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
  QByteArray message(std::max(1, length), '\0');
  GLsizei written = 0;
  gl.GetShaderInfoLog(shader, message.size(), &written, message.data());
  message.resize(std::max<GLsizei>(0, written));
  return QString::fromLocal8Bit(message);
}

QString programLog(LuDashGLDispatch &gl, GLuint program) {
  GLint length = 0;
  gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
  QByteArray message(std::max(1, length), '\0');
  GLsizei written = 0;
  gl.GetProgramInfoLog(program, message.size(), &written, message.data());
  message.resize(std::max<GLsizei>(0, written));
  return QString::fromLocal8Bit(message);
}

} // namespace

ShaderProgram::ShaderProgram(const LuDashGLDispatch &dispatch) : gl_(dispatch) {}

ShaderProgram::~ShaderProgram() {
  if (vertexArray_)
    gl_.DeleteVertexArrays(1, &vertexArray_);
  if (program_)
    gl_.DeleteProgram(program_);
}

std::unique_ptr<ShaderProgram>
ShaderProgram::create(const LuDashGLDispatch &dispatch,
                      const QList<ShaderAsset> &assets, QString *error) {
  auto program =
      std::unique_ptr<ShaderProgram>(new ShaderProgram(dispatch));
  if (!program->link(assets, error))
    return {};
  return program;
}

bool ShaderProgram::link(const QList<ShaderAsset> &assets, QString *error) {
  if (assets.isEmpty()) {
    if (error)
      *error = QStringLiteral("shader program has no stages");
    return false;
  }

  int computeCount = 0;
  bool hasVertex = false;
  QVector<GLuint> compiled;
  compiled.reserve(assets.size());

  for (const auto &asset : assets) {
    const GLenum stage = shaderStageGlEnum(asset.stage);
    if (!supported(stage)) {
      if (error)
        *error = QStringLiteral("unsupported shader stage: %1").arg(asset.name);
      return false;
    }
    computeCount += stage == GL_COMPUTE_SHADER ? 1 : 0;
    hasVertex = hasVertex || stage == GL_VERTEX_SHADER;

    const GLuint shader = gl_.CreateShader(stage);
    if (!shader) {
      if (error)
        *error = QStringLiteral("glCreateShader returned 0 for %1").arg(asset.name);
      return false;
    }
    const char *source = asset.source.constData();
    gl_.ShaderSource(shader, 1, &source, nullptr);
    gl_.CompileShader(shader);
    GLint ok = GL_FALSE;
    gl_.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      if (error)
        *error = QStringLiteral("%1: %2").arg(asset.name, shaderLog(gl_, shader));
      gl_.DeleteShader(shader);
      for (const GLuint compiledShader : compiled)
        gl_.DeleteShader(compiledShader);
      return false;
    }
    compiled.push_back(shader);
  }

  if (computeCount && assets.size() != 1) {
    if (error)
      *error = QStringLiteral(
          "compute shaders cannot be linked with graphics stages");
    for (const GLuint shader : compiled)
      gl_.DeleteShader(shader);
    return false;
  }

  program_ = gl_.CreateProgram();
  if (!program_) {
    if (error)
      *error = QStringLiteral("glCreateProgram returned 0");
    for (const GLuint shader : compiled)
      gl_.DeleteShader(shader);
    return false;
  }

  for (const GLuint shader : compiled)
    gl_.AttachShader(program_, shader);
  gl_.LinkProgram(program_);
  for (const GLuint shader : compiled)
    gl_.DeleteShader(shader);

  GLint ok = GL_FALSE;
  gl_.GetProgramiv(program_, GL_LINK_STATUS, &ok);
  if (!ok) {
    if (error)
      *error = programLog(gl_, program_);
    return false;
  }

  if (hasVertex) {
    gl_.GenVertexArrays(1, &vertexArray_);
    if (!vertexArray_) {
      if (error)
        *error = QStringLiteral("could not create fullscreen vertex array");
      return false;
    }
  }
  return true;
}

LuDashGLDispatch &ShaderProgram::gl() { return gl_; }
const LuDashGLDispatch &ShaderProgram::gl() const { return gl_; }
GLuint ShaderProgram::id() const { return program_; }
GLuint ShaderProgram::vertexArray() const { return vertexArray_; }

GLint ShaderProgram::uniform(const char *name) const {
  return gl_.GetUniformLocation(program_, name);
}

void ShaderProgram::bind() const {
  gl_.UseProgram(program_);
  if (vertexArray_)
    gl_.BindVertexArray(vertexArray_);
}

void ShaderProgram::unbind() const {
  if (vertexArray_)
    gl_.BindVertexArray(0);
  gl_.UseProgram(0);
}

void ShaderProgram::drawFullscreen() const {
  gl_.DrawArrays(GL_TRIANGLES, 0, 3);
}

} // namespace LuDash
