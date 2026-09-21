#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include <QJsonArray>
#include <QSet>
#include <climits>
#include <cmath>

namespace LunaDash {
namespace {
bool integer(const QJsonValue &value, int low, int high) {
  const double n = value.toDouble();
  return value.isDouble() && std::isfinite(n) && n == std::floor(n) &&
         n >= low && n <= high;
}
QJsonObject rectJson(QRect rect) {
  return {{"x", rect.x()},
          {"y", rect.y()},
          {"width", rect.width()},
          {"height", rect.height()}};
}
} // namespace
QJsonObject windowAnimationProfile(PluginManager &plugins,
                                   const QJsonObject &preferences,
                                   const WindowTemplate &windowTemplate) {
  auto profile = windowTemplate.animation;
  const auto configured = configuredBuiltinSettings("window-animation");
  for (auto it = configured.begin(); it != configured.end(); ++it)
    profile[it.key()] = it.value();
  if (profile.value("duration").toInt() < 0)
    profile["duration"] = preferences.value("animationDuration");
  const auto schema =
      extensionTarget("window-animation").value("settings").toObject();
  profile =
      plugins.filter("window-animation", profile, {}, [&](const auto &output) {
        return output.size() == schema.size() &&
               output.value("duration").toInt(-1) >= 0 &&
               validateExtensionSettings(schema, output, nullptr);
      });
  if (!preferences.value("animations").toBool())
    profile["duration"] = 0;
  return profile;
}
int shellAnimationDuration(PluginManager &plugins, int fallback) {
  const int configured =
      configuredBuiltinSettings("shell-animation").value("duration").toInt(-1);
  return plugins
      .filter("shell-animation",
              {{"duration", configured < 0 ? fallback : configured}}, {},
              [](const auto &output) {
                return output.size() == 1 &&
                       integer(output.value("duration"), 0, 600);
              })
      .value("duration")
      .toInt(fallback);
}
QJsonObject initialWindowRule(PluginManager &plugins,
                              const QJsonObject &context, int workspace,
                              bool maximized, int workspaceCount) {
  return plugins.filter(
      "window-rules", {{"workspace", workspace}, {"maximized", maximized}},
      context, [workspaceCount](const auto &output) {
        return output.size() == 2 &&
               integer(output.value("workspace"), 0, workspaceCount - 1) &&
               output.value("maximized").isBool();
      });
}
QList<WindowPlacement>
pluginWindowPlacements(PluginManager &plugins,
                       const WindowTemplate &windowTemplate,
                       LayoutWorkspaceId workspace, QRect area,
                       const QList<WindowPlacement> &placements) {
  QJsonArray windows;
  QJsonArray fresh;
  QSet<qint64> expected;
  for (const auto &placement : placements) {
    if (placement.minimized)
      continue;
    auto item = rectJson(placement.geometry);
    item["id"] = static_cast<qint64>(placement.window);
    windows.append(item);
    if (placement.newWindow)
      fresh.append(static_cast<qint64>(placement.window));
    expected.insert(static_cast<qint64>(placement.window));
  }
  const bool stacking =
      windowTemplate.layoutMode == WindowLayoutMode::Stacking;
  const auto output = plugins.filter(
      "window-layout", {{"windows", windows}},
      {{"workspace", static_cast<qint64>(workspace)},
       {"area", rectJson(area)},
       {"newWindows", fresh},
       {"windowTemplate", windowTemplate.key},
       {"layoutMode", stacking ? "stacking" : "tiling"}},
      [&](const auto &result) {
        if (result.size() != 1 || !result.value("windows").isArray() ||
            result.value("windows").toArray().size() != windows.size())
          return false;
        QSet<qint64> seen;
        QList<QRect> rectangles;
        for (const auto &value : result.value("windows").toArray()) {
          const auto object = value.toObject();
          if (object.size() != 5 || !integer(object.value("id"), 1, INT_MAX) ||
              !integer(object.value("x"), -32768, 32768) ||
              !integer(object.value("y"), -32768, 32768) ||
              !integer(object.value("width"), 1, 32768) ||
              !integer(object.value("height"), 1, 32768))
            return false;
          const qint64 id = object.value("id").toInteger();
          const QRect rect(object.value("x").toInt(), object.value("y").toInt(),
                           object.value("width").toInt(),
                           object.value("height").toInt());
          if (!expected.contains(id) || seen.contains(id) ||
              !area.contains(rect))
            return false;
          for (const auto &previous : rectangles)
            if (!stacking && previous.intersects(rect))
              return false;
          seen.insert(id);
          rectangles.append(rect);
        }
        return seen == expected;
      });
  auto result = placements;
  for (const auto &value : output.value("windows").toArray()) {
    const auto object = value.toObject();
    for (auto &placement : result)
      if (placement.window ==
          static_cast<LayoutWindowId>(object.value("id").toInteger())) {
        placement.geometry = QRect(
            object.value("x").toInt(), object.value("y").toInt(),
            object.value("width").toInt(), object.value("height").toInt());
        placement.width = placement.geometry.width();
      }
  }
  return result;
}
} // namespace LunaDash
