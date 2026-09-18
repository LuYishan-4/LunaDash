#pragma once

// Build-time information shared by LunaDash subsystems. Values are injected by
// CMake and deliberately remain macros because they must be available during
// preprocessing as well as C++ compilation.
#ifndef LUDASH_VERSION
#define LUDASH_VERSION "1.0.0"
#endif

#ifndef LUDASH_GIT_COMMIT
#define LUDASH_GIT_COMMIT "unknown"
#endif

#ifndef LUDASH_QML_SOURCE_DIR
#define LUDASH_QML_SOURCE_DIR ""
#endif

#ifndef LUDASH_SCRIPT_SOURCE_DIR
#define LUDASH_SCRIPT_SOURCE_DIR ""
#endif

#ifndef LUDASH_ASSET_SOURCE_DIR
#define LUDASH_ASSET_SOURCE_DIR ""
#endif
