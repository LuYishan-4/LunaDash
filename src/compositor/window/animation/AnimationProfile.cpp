#include "compositor/window/animation/AnimationProfile.hpp"

#include "config/desktop/PreferenceKeys.hpp"
#include "core/json/JsonAccess.hpp"

#include <cmath>

namespace LunaDash {
namespace {

SceneSpring springFrom(const nlohmann::json &motion, const char *name,
                       SceneSpring spring, double stiffnessScale) {
  const auto object = jsonObject(motion, name);
  spring.stiffness = jsonDouble(object, "stiffness", spring.stiffness);
  spring.dampingRatio = jsonDouble(object, "dampingRatio", spring.dampingRatio);
  spring.restDistance = jsonDouble(object, "restDistance", spring.restDistance);
  spring.restSpeed = jsonDouble(object, "restSpeed", spring.restSpeed);
  spring.stiffness *= stiffnessScale;
  return spring;
}

} // namespace

SceneAnimationRectangle animationRectangle(const Rect &rectangle) {
  return {rectangle.x(), rectangle.y(), rectangle.width(), rectangle.height()};
}

SceneAnimationOptions sceneAnimationOptions(const nlohmann::json &profile,
                                            const nlohmann::json &motion) {
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

  // Motion is tuned for the default animation duration. A spring's settle
  // time scales with 1 / sqrt(stiffness).
  const double timeScale =
      options.duration > 0
          ? static_cast<double>(options.duration) /
                Preference::animationDuration.fallback
          : 1.0;
  const double stiffnessScale = 1.0 / (timeScale * timeScale);
  options.openScale = jsonDouble(motion, "openScale", options.openScale);
  options.closeDuration = static_cast<int>(std::lround(
      jsonInt(motion, "closeDuration", options.closeDuration) * timeScale));
  options.windowOpen =
      springFrom(motion, "windowOpen", options.windowOpen, stiffnessScale);
  options.windowMovement =
      springFrom(motion, "windowMovement", options.windowMovement, stiffnessScale);
  options.windowResize =
      springFrom(motion, "windowResize", options.windowResize, stiffnessScale);
  options.workspaceSwitch = springFrom(motion, "workspaceSwitch",
                                       options.workspaceSwitch, stiffnessScale);
  return options;
}

} // namespace LunaDash
