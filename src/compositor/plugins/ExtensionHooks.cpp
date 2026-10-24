#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include <climits>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace LunaDash {
namespace {
bool integer(const Json &value, int low, int high) {
  const double n = jsonAsDouble(value);
  return value.is_number() && std::isfinite(n) && n == std::floor(n) &&
         n >= low && n <= high;
}
Json rectJson(Rect rect) {
  return {{"x", rect.x()},
          {"y", rect.y()},
          {"width", rect.width()},
          {"height", rect.height()}};
}
Rect rectFromJson(const Json &object) {
  return {jsonInt(object, "x"), jsonInt(object, "y"), jsonInt(object, "width"),
          jsonInt(object, "height")};
}
} // namespace
Json windowAnimationProfile(PluginManager &plugins, const Json &preferences,
                            const WindowTemplate &windowTemplate) {
  auto profile = jsonAsObject(windowTemplate.animation);
  const auto configured = configuredBuiltinSettings("window-animation");
  for (auto it = configured.begin(); it != configured.end(); ++it)
    profile[it.key()] = it.value();
  if (jsonInt(profile, "duration") < 0)
    profile["duration"] = jsonValue(preferences, "animationDuration");
  const auto schema = jsonObject(extensionTarget("window-animation"), "settings");
  profile = plugins.filter(
      "window-animation", profile, Json::object(), [&](const Json &output) {
        return output.size() == schema.size() &&
               jsonInt(output, "duration", -1) >= 0 &&
               validateExtensionSettings(schema, output, nullptr);
      });
  if (!jsonBool(preferences, "animations"))
    profile["duration"] = 0;
  return profile;
}
int shellAnimationDuration(PluginManager &plugins, int fallback) {
  const int configured =
      jsonInt(configuredBuiltinSettings("shell-animation"), "duration", -1);
  const auto output = plugins.filter(
      "shell-animation",
      Json{{"duration", configured < 0 ? fallback : configured}},
      Json::object(), [](const Json &result) {
        return result.size() == 1 && integer(jsonValue(result, "duration"), 0, 600);
      });
  return jsonInt(output, "duration", fallback);
}
Json initialWindowRule(PluginManager &plugins, const Json &context,
                       int workspace, bool maximized, int workspaceCount) {
  return plugins.filter(
      "window-rules", Json{{"workspace", workspace}, {"maximized", maximized}},
      context, [workspaceCount](const Json &output) {
        return output.size() == 2 &&
               integer(jsonValue(output, "workspace"), 0, workspaceCount - 1) &&
               jsonValue(output, "maximized").is_boolean();
      });
}
std::vector<WindowPlacement>
pluginWindowPlacements(PluginManager &plugins,
                       const WindowTemplate &windowTemplate,
                       LayoutWorkspaceId workspace, Rect area,
                       const std::vector<WindowPlacement> &placements) {
  Json windows = Json::array();
  Json fresh = Json::array();
  std::unordered_set<std::int64_t> expected;
  for (const auto &placement : placements) {
    if (placement.minimized)
      continue;
    auto item = rectJson(placement.geometry);
    item["id"] = static_cast<std::int64_t>(placement.window);
    windows.push_back(item);
    if (placement.newWindow)
      fresh.push_back(static_cast<std::int64_t>(placement.window));
    expected.insert(static_cast<std::int64_t>(placement.window));
  }
  const bool stacking = windowTemplate.allowOverlap;
  const auto output = plugins.filter(
      "window-layout", Json{{"windows", windows}},
      Json{{"workspace", static_cast<std::int64_t>(workspace)},
           {"area", rectJson(area)},
           {"newWindows", fresh},
           {"windowTemplate", windowTemplate.key},
           {"allowOverlap", stacking}},
      [&](const Json &result) {
        const auto values = jsonValue(result, "windows");
        if (result.size() != 1 || !values.is_array() ||
            values.size() != windows.size())
          return false;
        std::unordered_set<std::int64_t> seen;
        std::vector<Rect> rectangles;
        for (const auto &object : values) {
          if (!object.is_object() || object.size() != 5 ||
              !integer(jsonValue(object, "id"), 1, INT_MAX) ||
              !integer(jsonValue(object, "x"), -32768, 32768) ||
              !integer(jsonValue(object, "y"), -32768, 32768) ||
              !integer(jsonValue(object, "width"), 1, 32768) ||
              !integer(jsonValue(object, "height"), 1, 32768))
            return false;
          const std::int64_t id = jsonInteger(object, "id");
          const Rect rect = rectFromJson(object);
          if (!expected.contains(id) || seen.contains(id) ||
              !area.contains(rect))
            return false;
          for (const auto &previous : rectangles)
            if (!stacking && previous.intersects(rect))
              return false;
          seen.insert(id);
          rectangles.push_back(rect);
        }
        return seen == expected;
      });
  auto result = placements;
  for (const auto &object : jsonArray(output, "windows")) {
    for (auto &placement : result)
      if (placement.window ==
          static_cast<LayoutWindowId>(jsonInteger(object, "id"))) {
        placement.geometry = rectFromJson(object);
        placement.width = placement.geometry.width();
      }
  }
  return result;
}
} // namespace LunaDash
