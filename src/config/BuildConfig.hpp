#pragma once

#include "core/Defines.hpp"

#ifndef LUDASH_QML_SOURCE_DIR
#define LUDASH_QML_SOURCE_DIR ""
#endif

#ifndef LUDASH_SCRIPT_SOURCE_DIR
#define LUDASH_SCRIPT_SOURCE_DIR ""
#endif

#ifndef LUDASH_ASSET_SOURCE_DIR
#define LUDASH_ASSET_SOURCE_DIR ""
#endif

namespace LunaDash::BuildConfig {
inline constexpr bool hasXkbRegistry = LUDASH_HAS_XKBREGISTRY != 0;
inline constexpr bool rendererOpenGL = LUDASH_RENDERER_OPENGL != 0;
inline constexpr const char *version = LUDASH_VERSION;
inline constexpr const char *commit = LUDASH_GIT_COMMIT;
} // namespace LunaDash::BuildConfig
