#pragma once
#include "core/Defines.hpp"

#if LUDASH_RENDERER_OPENGL
#include "compositor/renderer/opengl/OpenGL.hpp"
namespace LunaDash {
using ActiveGraphics = OpenGL;
}
#else
#error "No LunaDash renderer selected: link the ludash-renderer target"
#endif
