#include "compositor/window/WindowTemplate.hpp"
#include "compositor/layout/FreeformLayout.hpp"
#include "compositor/tiling/TilingLayout.hpp"

#include <QJsonArray>
#include <cmath>

namespace LunaDash {
namespace {
std::unique_ptr<WindowLayout> createTilingLayout() {
  return std::make_unique<TilingLayout>();
}
std::unique_ptr<WindowLayout> createFreeformLayout() {
  return std::make_unique<FreeformLayout>();
}

QJsonObject defaults(const QJsonObject &schema) {
  QJsonObject result;
  for (auto it = schema.begin(); it != schema.end(); ++it)
    result[it.key()] = it.value().toObject().value("default");
  return result;
}

QJsonObject defaultWindowAnimation() {
  return {{"duration", -1},
          {"enterOffset", 12},
          {"focusOpacity", 0.82},
          {"exitScale", 0.90},
          {"easing", "outCubic"}};
}

QJsonObject action(const QString &label) {
  return {{"label", label}};
}

const WindowTemplate &tilingTemplate() {
  static const QJsonObject schema{
      {"gap", QJsonObject{{"type", "integer"},
                           {"default", 12},
                           {"minimum", 0},
                           {"maximum", 64},
                           {"label", "Window gaps"}}},
      {"defaultWidth", QJsonObject{{"type", "integer"},
                                    {"default", 960},
                                    {"minimum", 240},
                                    {"maximum", 2400},
                                    {"label", "Default window width"}}}};
  static const QJsonObject actions{
      {"focus-direction", action("Directional focus")},
      {"group-direction", action("Group in direction")},
      {"group-with", action("Group with window")},
      {"expel", action("Expel from group")},
      {"swap", action("Swap windows")},
      {"insert-beside", action("Insert beside window")},
      {"reorder", action("Reorder window")},
      {"resize-width", action("Resize width")},
      {"resize-height", action("Resize height")},
      {"move-by", action("Move window")},
      {"center", action("Center window")}};
  static const WindowTemplate value{
      "tiling", &createTilingLayout, WindowPointerTemplate::Tiling,
      WindowActivationTemplate::ToggleMaximize, false, false, schema,
      defaults(schema), actions, defaultWindowAnimation()};
  return value;
}

const WindowTemplate &stackingTemplate() {
  static const QJsonObject schema{
      {"defaultWidth", QJsonObject{{"type", "integer"},
                                    {"default", 900},
                                    {"minimum", 240},
                                    {"maximum", 2400},
                                    {"label", "Default window width"}}},
      {"defaultHeight", QJsonObject{{"type", "integer"},
                                     {"default", 600},
                                     {"minimum", 160},
                                     {"maximum", 1600},
                                     {"label", "Default window height"}}}};
  static const QJsonObject actions{
      {"focus-direction", action("Directional focus")},
      {"swap", action("Swap windows")},
      {"reorder", action("Reorder window")},
      {"resize-width", action("Resize width")},
      {"resize-height", action("Resize height")},
      {"move-by", action("Move window")},
      {"center", action("Center window")}};
  static const WindowTemplate value{
      "stacking", &createFreeformLayout, WindowPointerTemplate::Freeform,
      WindowActivationTemplate::FocusOnly, true, true, schema,
      defaults(schema), actions, defaultWindowAnimation()};
  return value;
}
} // namespace

QList<WindowTemplate> windowTemplates() {
  return {tilingTemplate(), stackingTemplate()};
}

const WindowTemplate &windowTemplateForKey(const QString &key) {
  if (key == "stacking")
    return stackingTemplate();
  return tilingTemplate();
}

std::unique_ptr<WindowLayout>
createWindowLayout(const WindowTemplate &windowTemplate) {
  return windowTemplate.createLayout ? windowTemplate.createLayout() : nullptr;
}

bool validateWindowLayoutSettings(const WindowTemplate &windowTemplate,
                                  const QJsonObject &settings,
                                  QString *error) {
  for (auto it = settings.begin(); it != settings.end(); ++it) {
    const auto rule = windowTemplate.layoutSettingsSchema.value(it.key()).toObject();
    if (rule.isEmpty()) {
      if (error)
        *error = "Unknown window layout setting: " + it.key();
      return false;
    }
    const auto type = rule.value("type").toString();
    const auto value = it.value();
    bool valid = false;
    if (type == "boolean") {
      valid = value.isBool();
    } else if (type == "string") {
      valid = value.isString() && value.toString().size() <= 4096;
    } else if (type == "integer" || type == "number") {
      const double number = value.toDouble();
      valid = value.isDouble() && std::isfinite(number) &&
              (type != "integer" || number == std::floor(number)) &&
              (!rule.contains("minimum") ||
               number >= rule.value("minimum").toDouble()) &&
              (!rule.contains("maximum") ||
               number <= rule.value("maximum").toDouble());
    }
    if (valid && rule.contains("enum"))
      valid = rule.value("enum").toArray().contains(value);
    if (!valid) {
      if (error)
        *error = "Invalid window layout setting: " + it.key();
      return false;
    }
  }
  return true;
}

bool windowTemplateSupportsAction(const WindowTemplate &windowTemplate,
                                  const QString &actionId) {
  return windowTemplate.layoutActions.contains(actionId);
}

bool performWindowLayoutAction(WindowLayout &layout,
                               const WindowTemplate &windowTemplate,
                               const QString &actionId,
                               const QJsonObject &payload) {
  return windowTemplateSupportsAction(windowTemplate, actionId) &&
         layout.performAction(actionId, payload);
}

} // namespace LunaDash
