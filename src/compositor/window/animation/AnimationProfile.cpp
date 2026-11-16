#include "compositor/window/animation/AnimationProfile.hpp"

#include "core/json/JsonAccess.hpp"

namespace LunaDash {

SceneAnimationRectangle animationRectangle(const Rect &rectangle) {
  return {rectangle.x(), rectangle.y(), rectangle.width(), rectangle.height()};
}

SceneAnimationOptions sceneAnimationOptions(const nlohmann::json &profile) {
  SceneAnimationOptions options;
  options.duration = jsonInt(profile, "duration", options.duration);
  options.enterOffset = jsonInt(profile, "enterOffset", options.enterOffset);
  options.focusOpacity =
      jsonDouble(profile, "focusOpacity", options.focusOpacity);
  options.exitScale = jsonDouble(profile, "exitScale", options.exitScale);
  const auto curve = jsonString(profile, "easing");
  if (curve == "linear")
    options.curve = SceneAnimationCurve::Linear;
  else if (curve == "outQuint")
    options.curve = SceneAnimationCurve::OutQuint;
  else if (curve == "inOutCubic")
    options.curve = SceneAnimationCurve::InOutCubic;
  return options;
}

} // namespace LunaDash
