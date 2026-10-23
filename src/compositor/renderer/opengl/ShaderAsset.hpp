#pragma once

#include <GL/glcorearb.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
  std::string name;
  ShaderStage stage = ShaderStage::Unknown;
  std::string source;
};

ShaderStage shaderStageFromName(std::string_view name);
GLenum shaderStageGlEnum(ShaderStage stage);

class ShaderAssetLoader final {
public:
  static std::optional<ShaderAsset> load(std::string_view name, bool openGLES,
                                         std::string *error = nullptr);
  static std::vector<ShaderAsset>
  loadMany(const std::vector<std::string> &names, bool openGLES,
           std::string *error = nullptr);
};

} // namespace LunaDash
