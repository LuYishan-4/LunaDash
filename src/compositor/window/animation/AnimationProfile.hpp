#pragma once

#include "compositor/window/animation/SceneAnimation.hpp"
#include "core/geometry/Geometry.hpp"

#include <nlohmann/json.hpp>

namespace LunaDash {

SceneAnimationRectangle animationRectangle(const Rect &rectangle);
// Scene animation options from a window-animation profile (template or plugin).
SceneAnimationOptions sceneAnimationOptions(const nlohmann::json &profile);

} // namespace LunaDash
