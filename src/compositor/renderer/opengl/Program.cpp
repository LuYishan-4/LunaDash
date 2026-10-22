#include "compositor/renderer/opengl/Program.hpp"

#include "compositor/renderer/opengl/Shader.hpp"
#include <QByteArray>
#include <algorithm>
#include <limits>
#include <vector>

namespace LunaDash {
namespace {

QString programLog(LuDashGLDispatch &gl, GLuint program) {
  GLint length = 0;
  gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
  const GLsizei capacity = std::max(1, length);
  QByteArray message(capacity, '\0');
  GLsizei written = 0;
  gl.GetProgramInfoLog(program, capacity, &written, message.data());
  message.resize(std::max<GLsizei>(0, written));
  return QString::fromLocal8Bit(message);
}

} // namespace

Program::Program(const LuDashGLDispatch &dispatch) : gl_(dispatch) {}

Program::~Program() {
  if (vertexArray_)
    gl_.DeleteVertexArrays(1, &vertexArray_);
  if (program_)
    gl_.DeleteProgram(program_);
}

std::unique_ptr<Program> Program::create(const LuDashGLDispatch &dispatch,
                                         const QList<ShaderAsset> &assets,
                                         QString *error) {
  auto program = std::unique_ptr<Program>(new Program(dispatch));
  if (!program->link(assets, error))
    return {};
  return program;
}

bool Program::link(const QList<ShaderAsset> &assets, QString *error) {
  if (assets.isEmpty()) {
    if (error)
      *error = QStringLiteral("shader program has no stages");
    return false;
  }

  int computeCount = 0;
  bool hasVertex = false;
  std::vector<std::unique_ptr<Shader>> compiled;
  compiled.reserve(assets.size());

  for (const auto &asset : assets) {
    const GLenum stage = shaderStageGlEnum(asset.stage);
    computeCount += stage == GL_COMPUTE_SHADER ? 1 : 0;
    hasVertex = hasVertex || stage == GL_VERTEX_SHADER;
    auto shader = Shader::compile(gl_, asset, error);
    if (!shader)
      return false;
    compiled.push_back(std::move(shader));
  }

  if (computeCount && assets.size() != 1) {
    if (error)
      *error = QStringLiteral(
          "compute shaders cannot be linked with graphics stages");
    return false;
  }

  program_ = gl_.CreateProgram();
  if (!program_) {
    if (error)
      *error = QStringLiteral("glCreateProgram returned 0");
    return false;
  }

  for (const auto &shader : compiled)
    gl_.AttachShader(program_, shader->id());
  gl_.LinkProgram(program_);
  compiled.clear();

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

LuDashGLDispatch &Program::gl() { return gl_; }
const LuDashGLDispatch &Program::gl() const { return gl_; }
GLuint Program::id() const { return program_; }
GLuint Program::vertexArray() const { return vertexArray_; }

GLint Program::uniform(const char *name) const {
  return gl_.GetUniformLocation(program_, name);
}

void Program::bind() const {
  gl_.UseProgram(program_);
  if (vertexArray_)
    gl_.BindVertexArray(vertexArray_);
}

void Program::unbind() const {
  if (vertexArray_)
    gl_.BindVertexArray(0);
  gl_.UseProgram(0);
}

void Program::drawFullscreen() const { gl_.DrawArrays(GL_TRIANGLES, 0, 3); }

} // namespace LunaDash
