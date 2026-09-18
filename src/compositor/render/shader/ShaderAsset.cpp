#include "compositor/render/shader/ShaderAsset.hpp"

#include <QFile>
#include <QFileInfo>

int qInitResources_renderer_shaders();

namespace LuDash {
namespace {

void ensureShaderResources() {
  static const bool initialized = [] {
    ::qInitResources_renderer_shaders();
    return true;
  }();
  Q_UNUSED(initialized);
}

ShaderStage stageFromToken(QString token) {
  token = token.trimmed().toLower();
  if (token == "vert" || token == "vertex" || token == "vsh" || token == "vs")
    return ShaderStage::Vertex;
  if (token == "frag" || token == "fragment" || token == "fsh" || token == "fs")
    return ShaderStage::Fragment;
  if (token == "geom" || token == "geometry" || token == "gsh" || token == "gs")
    return ShaderStage::Geometry;
  if (token == "comp" || token == "compute" || token == "csh" || token == "cs")
    return ShaderStage::Compute;
  if (token == "tesc" || token == "tesscontrol" || token == "tess_control")
    return ShaderStage::TessControl;
  if (token == "tese" || token == "tesseval" || token == "tess_eval")
    return ShaderStage::TessEvaluation;
  return ShaderStage::Unknown;
}

ShaderStage stageFromSourceDirective(const QByteArray &source) {
  const QList<QByteArray> lines = source.left(2048).split('\n');
  for (QByteArray line : lines) {
    line = line.trimmed();
    static const QByteArray pragma = "#pragma ludash_stage";
    if (line.startsWith(pragma))
      return stageFromToken(
          QString::fromLatin1(line.mid(pragma.size()).trimmed()));
  }
  return ShaderStage::Unknown;
}

QByteArray versionPrefix(ShaderStage stage, bool openGLES) {
  if (openGLES) {
    switch (stage) {
    case ShaderStage::Compute:
      return "#version 310 es\nprecision highp float;\nprecision highp int;\n";
    case ShaderStage::Geometry:
    case ShaderStage::TessControl:
    case ShaderStage::TessEvaluation:
      return "#version 320 es\nprecision highp float;\nprecision highp int;\n";
    case ShaderStage::Vertex:
    case ShaderStage::Fragment:
    case ShaderStage::Unknown:
      return "#version 300 es\nprecision highp float;\nprecision highp int;\n";
    }
  }

  switch (stage) {
  case ShaderStage::Compute:
    return "#version 430 core\n";
  case ShaderStage::TessControl:
  case ShaderStage::TessEvaluation:
    return "#version 400 core\n";
  case ShaderStage::Vertex:
  case ShaderStage::Fragment:
  case ShaderStage::Geometry:
  case ShaderStage::Unknown:
    return "#version 330 core\n";
  }
  return {};
}

bool hasVersionDirective(QByteArray source) {
  if (source.startsWith("\xEF\xBB\xBF"))
    source.remove(0, 3);
  return source.trimmed().startsWith("#version");
}

bool genericSuffix(const QString &suffix) {
  return suffix == "glsl" || suffix == "shader" || suffix == "glal";
}

} // namespace

ShaderStage shaderStageFromName(const QString &name) {
  QString fileName = QFileInfo(name).fileName().toLower();
  QString suffix = QFileInfo(fileName).suffix();
  if (genericSuffix(suffix)) {
    fileName.chop(suffix.size() + 1);
    suffix = QFileInfo(fileName).suffix();
  }
  return stageFromToken(suffix);
}

GLenum shaderStageGlEnum(ShaderStage stage) {
  switch (stage) {
  case ShaderStage::Vertex:
    return GL_VERTEX_SHADER;
  case ShaderStage::Fragment:
    return GL_FRAGMENT_SHADER;
  case ShaderStage::Geometry:
    return GL_GEOMETRY_SHADER;
  case ShaderStage::Compute:
    return GL_COMPUTE_SHADER;
  case ShaderStage::TessControl:
    return GL_TESS_CONTROL_SHADER;
  case ShaderStage::TessEvaluation:
    return GL_TESS_EVALUATION_SHADER;
  case ShaderStage::Unknown:
    return 0;
  }
  return 0;
}

std::optional<ShaderAsset>
ShaderAssetLoader::load(const QString &name, bool openGLES, QString *error) {
  ensureShaderResources();
  if (error)
    error->clear();

  QFile file(":/LuDash/data/shaders/" + name);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error)
      *error = QStringLiteral("could not open shader asset: %1").arg(name);
    return std::nullopt;
  }

  ShaderAsset asset;
  asset.name = name;
  asset.source = file.readAll();
  asset.stage = shaderStageFromName(name);
  if (asset.stage == ShaderStage::Unknown)
    asset.stage = stageFromSourceDirective(asset.source);

  const QString suffix = QFileInfo(name).suffix().toLower();
  if (genericSuffix(suffix) && asset.stage == ShaderStage::Unknown) {
    if (error)
      *error = QStringLiteral(
                   "%1 needs a stage suffix or #pragma ludash_stage")
                   .arg(name);
    return std::nullopt;
  }
  if (asset.stage == ShaderStage::Unknown) {
    if (error)
      *error = QStringLiteral("unrecognized shader stage: %1").arg(name);
    return std::nullopt;
  }

  if (!hasVersionDirective(asset.source))
    asset.source.prepend(versionPrefix(asset.stage, openGLES));
  return asset;
}

QList<ShaderAsset> ShaderAssetLoader::loadMany(const QStringList &names,
                                               bool openGLES,
                                               QString *error) {
  QList<ShaderAsset> assets;
  assets.reserve(names.size());
  for (const auto &name : names) {
    auto asset = load(name, openGLES, error);
    if (!asset)
      return {};
    assets.push_back(std::move(*asset));
  }
  return assets;
}

} // namespace LuDash
