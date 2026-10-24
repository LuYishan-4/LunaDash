#include "compositor/window/WindowTemplate.hpp"
#include "compositor/layout/FreeformLayout.hpp"
#include "compositor/tiling/TilingLayout.hpp"

#include "core/settings/SettingsSchema.hpp"

namespace LunaDash {
namespace {
std::unique_ptr<WindowLayout> createTilingLayout() {
  return std::make_unique<TilingLayout>();
}
std::unique_ptr<WindowLayout> createFreeformLayout() {
  return std::make_unique<FreeformLayout>();
}

Json defaults(const Json &schema) {
  Json result = Json::object();
  for (auto it = schema.begin(); it != schema.end(); ++it)
    result[it.key()] = jsonValue(it.value(), "default");
  return result;
}

Json defaultWindowAnimation() {
  return {{"duration", -1},
          {"enterOffset", 8},
          {"focusOpacity", 0.88},
          {"exitScale", 0.96},
          {"easing", "outCubic"}};
}

Json numberParameter(int minimum, int maximum, int value = 0) {
  return {{"type", "integer"}, {"minimum", minimum}, {"maximum", maximum},
          {"default", value}};
}
Json action(const std::string &label, Json parameters, bool area = false,
                   bool cardinal = false) {
  Json required = Json::array();
  for (auto it = parameters.begin(); it != parameters.end(); ++it)
    required.push_back(it.key());
  return {{"label", label}, {"parameters", parameters}, {"required", required},
          {"requiresArea", area}, {"cardinalDirection", cardinal}};
}
Json commonActions() {
  const auto window = numberParameter(1, 2147483647, 1);
  const auto workspace = numberParameter(0, 2147483647);
  const auto delta = numberParameter(-32768, 32768);
  const auto size = numberParameter(1, 32768, 1);
  auto direction = numberParameter(-1, 1, 1);
  direction["enum"] = Json::array({-1, 1});
  return {
      {"focus-direction", action("Directional focus", {{"workspace", workspace},
          {"dx", numberParameter(-1, 1)}, {"dy", numberParameter(-1, 1)}}, false, true)},
      {"swap", action("Swap windows", {{"window", window}, {"target", window}})},
      {"reorder", action("Reorder window", {{"window", window}, {"direction", direction}})},
      {"resize-width", action("Resize width", {{"window", window}, {"width", size}})},
      {"resize-height", action("Resize height", {{"window", window}, {"height", size}})},
      {"move-by", action("Move window", {{"window", window}, {"dx", delta}, {"dy", delta}}, true)},
      {"center", action("Center window", {{"window", window}}, true)}};
}
Json tilingActions() {
  auto actions = commonActions();
  const auto window = numberParameter(1, 2147483647, 1);
  actions["group-direction"] = action("Group in direction",
      {{"window", window}, {"workspace", numberParameter(0, 2147483647)},
       {"dx", numberParameter(-1, 1)}, {"dy", numberParameter(-1, 1)}}, false, true);
  actions["group-with"] = action("Group with window", {{"window", window}, {"target", window}});
  actions["expel"] = action("Expel from group", {{"window", window}});
  actions["insert-beside"] = action("Insert beside window", {{"window", window}, {"target", window},
      {"after", Json::object({{"type", "boolean"}, {"default", true}})}});
  return actions;
}

const WindowTemplate &tilingTemplate() {
  static const Json schema{
      {"gap", Json::object({{"type", "integer"},
                           {"default", 12},
                           {"minimum", 0},
                           {"maximum", 64},
                           {"label", "Window gaps"}})},
      {"defaultWidth", Json::object({{"type", "integer"},
                                    {"default", 1120},
                                    {"minimum", 240},
                                    {"maximum", 2400},
                                    {"label", "Default window width"}})},
      {"minimumTileWidth", Json::object({{"type", "integer"},
                                        {"default", 320},
                                        {"minimum", 1},
                                        {"maximum", 1200},
                                        {"label", "Preferred minimum tile width"},
                                        {"description", "Use a larger tile when a new split would be too narrow. Crowded outputs may use smaller tiles."}})},
      {"minimumTileHeight", Json::object({{"type", "integer"},
                                         {"default", 300},
                                         {"minimum", 1},
                                         {"maximum", 900},
                                         {"label", "Preferred minimum tile height"},
                                         {"description", "Use a larger tile when a new split would be too short. Crowded outputs may use smaller tiles."}})},
      {"splitTarget", Json::object({{"type", "string"},
                                   {"default", "focused"},
                                   {"enum", Json{"focused", "largest"}},
                                   {"label", "Split new windows into"},
                                   {"description", "Focused divides the active tile and alternates split direction. Largest divides the largest tile."},
                                   {"control", "select"}})},
      {"firstWindowSide", Json::object({{"type", "string"},
                                       {"default", "right"},
                                       {"enum", Json{"right", "left"}},
                                       {"label", "First window side"},
                                       {"description", "Keep the first window on this side when a second window opens. Changes apply to future splits."},
                                       {"control", "select"}})}};
  static const auto actions = tilingActions();
  static const WindowTemplate value{
      "tiling", &createTilingLayout, false, false, true, schema,
      defaults(schema), actions, defaultWindowAnimation()};
  return value;
}

const WindowTemplate &stackingTemplate() {
  static const Json schema{
      {"defaultWidth", Json::object({{"type", "integer"},
                                    {"default", 900},
                                    {"minimum", 240},
                                    {"maximum", 2400},
                                    {"label", "Default window width"}})},
      {"defaultHeight", Json::object({{"type", "integer"},
                                     {"default", 600},
                                     {"minimum", 160},
                                     {"maximum", 1600},
                                     {"label", "Default window height"}})}};
  static const auto actions = commonActions();
  static const WindowTemplate value{
      "stacking", &createFreeformLayout, true, true, false, schema,
      defaults(schema), actions, defaultWindowAnimation()};
  return value;
}
} // namespace

std::vector<WindowTemplate> windowTemplates() {
  return {tilingTemplate(), stackingTemplate()};
}

const WindowTemplate &windowTemplateForKey(const std::string &key) {
  if (key == "stacking")
    return stackingTemplate();
  return tilingTemplate();
}

std::unique_ptr<WindowLayout>
createWindowLayout(const WindowTemplate &windowTemplate) {
  return windowTemplate.createLayout ? windowTemplate.createLayout() : nullptr;
}

bool validateWindowLayoutSettings(const WindowTemplate &windowTemplate,
                                  const Json &settings,
                                  std::string *error) {
  return Settings::validateSchema(windowTemplate.layoutSettingsSchema, error) &&
         Settings::validateValues(windowTemplate.layoutSettingsSchema, settings, error);
}

bool windowTemplateSupportsAction(const WindowTemplate &windowTemplate,
                                  const std::string &actionId) {
  return windowTemplate.layoutActions.contains(actionId);
}

bool performWindowLayoutAction(WindowLayout &layout,
                               const WindowTemplate &windowTemplate,
                               const std::string &actionId,
                               const Json &payload) {
  if (!windowTemplateSupportsAction(windowTemplate, actionId))
    return false;
  const auto descriptor = jsonObject(windowTemplate.layoutActions, actionId);
  const auto schema = jsonObject(descriptor, "parameters");
  auto parameters = payload;
  if (jsonBool(descriptor, "requiresArea")) {
    const auto area = jsonTake(parameters, "area");
    if (!area.is_object()) return false;
    const Json rectangleSchema{
        {"x", numberParameter(-32768, 32768)}, {"y", numberParameter(-32768, 32768)},
        {"width", numberParameter(1, 32768, 1)}, {"height", numberParameter(1, 32768, 1)}};
    if (area.size() != 4 ||
        !Settings::validateValues(rectangleSchema, area)) return false;
  }
  for (const auto &key : jsonArray(descriptor, "required"))
    if (!jsonContains(parameters, jsonAsString(key))) return false;
  if (!Settings::validateSchema(schema) || !Settings::validateValues(schema, parameters))
    return false;
  if (jsonBool(descriptor, "cardinalDirection")) {
    if (std::abs(jsonInt(parameters, "dx")) + std::abs(jsonInt(parameters, "dy")) != 1)
      return false;
    // Grouping must not move focus in a different workspace supplied by IPC.
    if (parameters.contains("window")) {
      const auto inventory = layout.snapshot(jsonInteger(parameters, "workspace")).columns;
      const auto id = jsonInteger(parameters, "window");
      if (std::none_of(inventory.cbegin(), inventory.cend(), [id](const auto &entry) {
            return entry.window == static_cast<LayoutWindowId>(id) && !entry.minimized;
          })) return false;
    }
  }
  return layout.performAction(actionId, payload);
}

} // namespace LunaDash
