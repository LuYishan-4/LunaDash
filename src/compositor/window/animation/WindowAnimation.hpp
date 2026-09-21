#pragma once

#include "compositor/window/animation/SceneAnimationBackend.hpp"
#include "core/templates/WindowAnimation.hpp"

namespace LunaDash {

using SceneWindowAnimationTemplate =
    Templates::WindowAnimationTemplate<SceneAnimationBackend>;

} // namespace LunaDash
