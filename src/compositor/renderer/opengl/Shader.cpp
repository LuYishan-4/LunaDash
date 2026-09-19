#include "compositor/renderer/opengl/Shader.hpp"
#include <algorithm>

namespace LunaDash {
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

} // namespace

Shader::Shader(const LuDashGLDispatch &dispatch, GLuint id)
    : gl_(dispatch), id_(id) {}
Shader::~Shader() {
  if (id_)
    gl_.DeleteShader(id_);
}
GLuint Shader::id() const { return id_; }

std::unique_ptr<Shader> Shader::compile(const LuDashGLDispatch &dispatch,
                                        const ShaderAsset &asset,
                                        QString *error) {
  const GLenum stage = shaderStageGlEnum(asset.stage);
  if (!supported(stage)) {
    if (error)
      *error = QStringLiteral("unsupported shader stage: %1").arg(asset.name);
    return {};
  }
  const GLuint id = dispatch.CreateShader(stage);
  if (!id) {
    if (error)
      *error =
          QStringLiteral("glCreateShader returned 0 for %1").arg(asset.name);
    return {};
  }
  auto shader = std::unique_ptr<Shader>(new Shader(dispatch, id));
  const char *source = asset.source.constData();
  dispatch.ShaderSource(id, 1, &source, nullptr);
  dispatch.CompileShader(id);
  GLint ok = GL_FALSE;
  dispatch.GetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    if (error)
      *error =
          QStringLiteral("%1: %2").arg(asset.name, shaderLog(shader->gl_, id));
    return {};
  }
  return shader;
}
} // namespace LunaDash
