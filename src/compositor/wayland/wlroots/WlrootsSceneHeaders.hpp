#pragma once

// Narrow wlroots scene boundary for C++ animation code. wlr_scene.h contains
// C99 array qualifiers which aren't accepted as C++ syntax.
#ifdef __cplusplus
#define static
extern "C" {
#endif

#include <wlr/types/wlr_scene.h>

#ifdef __cplusplus
}
#undef static
#endif
