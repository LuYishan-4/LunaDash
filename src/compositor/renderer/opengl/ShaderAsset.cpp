#include "compositor/renderer/opengl/ShaderAsset.hpp"

#include "ShaderResources.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <utility>

namespace LunaDash {
namespace {
using EmbeddedAsset = std::pair<std::string_view, std::string_view>;

constexpr std::array<EmbeddedAsset, 4> kEmbeddedShaders{{
    {"Fullscreen.vert", EmbeddedShaders::fullscreenVertex},
    {"Wallpaper.frag", EmbeddedShaders::wallpaperFragment},
    {"Blur.frag", EmbeddedShaders::blurFragment},
    {"Decoration.frag", EmbeddedShaders::decorationFragment},
}};

std::string lower(std::string_view value) {
  std::string result(value);
  std::transform(
      result.begin(), result.end(), result.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

std::string_view suffix(std::string_view name) {
  const auto slash = name.find_last_of('/');
  const auto filename =
      slash == std::string_view::npos ? name : name.substr(slash + 1);
  const auto dot = filename.find_last_of('.');
  return dot == std::string_view::npos ? std::string_view{}
                                       : filename.substr(dot + 1);
}

bool genericSuffix(std::string_view value) {
  const auto normalized = lower(value);
  return normalized == "glsl" || normalized == "shader" || normalized == "glal";
}

std::string_view stageToken(std::string_view value) {
  const auto normalized = lower(value);
  if (normalized == "vert" || normalized == "vertex" || normalized == "vsh" ||
      normalized == "vs")
    return "vertex";
  if (normalized == "frag" || normalized == "fragment" || normalized == "fsh" ||
      normalized == "fs")
    return "fragment";
  if (normalized == "geom" || normalized == "geometry" || normalized == "gsh" ||
      normalized == "gs")
    return "geometry";
  if (normalized == "comp" || normalized == "compute" || normalized == "csh" ||
      normalized == "cs")
    return "compute";
  if (normalized == "tesc" || normalized == "tesscontrol" ||
      normalized == "tess_control")
    return "tess-control";
  if (normalized == "tese" || normalized == "tesseval" ||
      normalized == "tess_eval")
    return "tess-evaluation";
  return {};
}

ShaderStage stageFromSourceDirective(std::string_view source) {
  const auto prefix =
      source.substr(0, std::min<std::size_t>(source.size(), 2048));
  std::size_t start = 0;
  while (start <= prefix.size()) {
    const auto end = prefix.find('\n', start);
    auto line = prefix.substr(start, end == std::string_view::npos
                                         ? prefix.size() - start
                                         : end - start);
    while (!line.empty() &&
           std::isspace(static_cast<unsigned char>(line.front())))
      line.remove_prefix(1);
    while (!line.empty() &&
           std::isspace(static_cast<unsigned char>(line.back())))
      line.remove_suffix(1);
    constexpr std::string_view pragma = "#pragma ludash_stage";
    if (line.starts_with(pragma)) {
      auto token = line.substr(pragma.size());
      while (!token.empty() &&
             std::isspace(static_cast<unsigned char>(token.front())))
        token.remove_prefix(1);
      const auto stage = stageToken(token);
      if (stage == "vertex")
        return ShaderStage::Vertex;
      if (stage == "fragment")
        return ShaderStage::Fragment;
      if (stage == "geometry")
        return ShaderStage::Geometry;
      if (stage == "compute")
        return ShaderStage::Compute;
      if (stage == "tess-control")
        return ShaderStage::TessControl;
      if (stage == "tess-evaluation")
        return ShaderStage::TessEvaluation;
    }
    if (end == std::string_view::npos)
      break;
    start = end + 1;
  }
  return ShaderStage::Unknown;
}

std::string versionPrefix(ShaderStage stage, bool openGLES) {
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

bool hasVersionDirective(std::string_view source) {
  if (source.starts_with("\xEF\xBB\xBF"))
    source.remove_prefix(3);
  while (!source.empty() &&
         std::isspace(static_cast<unsigned char>(source.front())))
    source.remove_prefix(1);
  return source.starts_with("#version");
}

} // namespace

ShaderStage shaderStageFromName(std::string_view name) {
  auto extension = suffix(name);
  if (genericSuffix(extension)) {
    const auto slash = name.find_last_of('/');
    const auto filename =
        slash == std::string_view::npos ? name : name.substr(slash + 1);
    const auto genericDot = filename.find_last_of('.');
    const auto withoutGeneric = genericDot == std::string_view::npos
                                    ? filename
                                    : filename.substr(0, genericDot);
    extension = suffix(withoutGeneric);
  }
  const auto stage = stageToken(extension);
  if (stage == "vertex")
    return ShaderStage::Vertex;
  if (stage == "fragment")
    return ShaderStage::Fragment;
  if (stage == "geometry")
    return ShaderStage::Geometry;
  if (stage == "compute")
    return ShaderStage::Compute;
  if (stage == "tess-control")
    return ShaderStage::TessControl;
  if (stage == "tess-evaluation")
    return ShaderStage::TessEvaluation;
  return ShaderStage::Unknown;
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

std::optional<ShaderAsset> ShaderAssetLoader::load(std::string_view name,
                                                   bool openGLES,
                                                   std::string *error) {
  if (error)
    error->clear();
  const auto embedded = std::find_if(
      kEmbeddedShaders.begin(), kEmbeddedShaders.end(),
      [name](const EmbeddedAsset &entry) { return entry.first == name; });
  if (embedded == kEmbeddedShaders.end()) {
    if (error)
      *error = "could not find embedded shader asset: " + std::string(name);
    return std::nullopt;
  }

  ShaderAsset asset{std::string(name), shaderStageFromName(name),
                    std::string(embedded->second)};
  if (asset.stage == ShaderStage::Unknown)
    asset.stage = stageFromSourceDirective(asset.source);
  if (genericSuffix(suffix(name)) && asset.stage == ShaderStage::Unknown) {
    if (error)
      *error =
          std::string(name) + " needs a stage suffix or #pragma ludash_stage";
    return std::nullopt;
  }
  if (asset.stage == ShaderStage::Unknown) {
    if (error)
      *error = "unrecognized shader stage: " + std::string(name);
    return std::nullopt;
  }
  if (!hasVersionDirective(asset.source))
    asset.source.insert(0, versionPrefix(asset.stage, openGLES));
  return asset;
}

std::vector<ShaderAsset>
ShaderAssetLoader::loadMany(const std::vector<std::string> &names,
                            bool openGLES, std::string *error) {
  std::vector<ShaderAsset> assets;
  assets.reserve(names.size());
  for (const auto &name : names) {
    auto asset = load(name, openGLES, error);
    if (!asset)
      return {};
    assets.push_back(std::move(*asset));
  }
  return assets;
}

} // namespace LunaDash
