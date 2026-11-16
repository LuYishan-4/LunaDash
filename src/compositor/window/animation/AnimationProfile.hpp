#pragma once

#include "compositor/window/animation/SceneAnimation.hpp"
#include "core/geometry/Geometry.hpp"

#include <nlohmann/json.hpp>

namespace LunaDash {

SceneAnimationRectangle animationRectangle(const Rect &rectangle);
// Scene animation options from a window-animation profile (template, user
// settings and plugins) and the template's motion (springs and timings). The
// motion is scaled to the profile's duration: a longer duration gives softer
// springs and a longer close animation.
SceneAnimationOptions sceneAnimationOptions(const nlohmann::json &profile,
                                            const nlohmann::json &motion);

} // namespace LunaDash
