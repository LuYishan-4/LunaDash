#pragma once

#include <GL/glcorearb.h>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace LunaDash {

enum class ShaderStage {
  Unknown,
  Vertex,
  Fragment,
  Geometry,
  Compute,
  TessControl,
  TessEvaluation,
};

struct ShaderAsset {
  QString name;
  ShaderStage stage = ShaderStage::Unknown;
  QByteArray source;
};

ShaderStage shaderStageFromName(const QString &name);
GLenum shaderStageGlEnum(ShaderStage stage);

class ShaderAssetLoader final {
public:
  static std::optional<ShaderAsset> load(const QString &name, bool openGLES,
                                         QString *error = nullptr);
  static QList<ShaderAsset> loadMany(const QStringList &names, bool openGLES,
                                     QString *error = nullptr);
};

} // namespace LunaDash
