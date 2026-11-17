#include "compositor/layout/FreeformLayout.hpp"
#include "compositor/layout/ScrollingLayout.hpp"
#include "compositor/tiling/TilingLayout.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/desktop/PreferenceKeys.hpp"

namespace LunaDash {
namespace {
// Tiling fills the screen with a split tree or keeps windows in scrollable
// columns; the tilingMode desktop preference picks the variant.
constexpr const char *tilingModePreference = Preference::tilingMode.name;

std::unique_ptr<WindowLayout> createTilingLayout() {
  return std::make_unique<TilingLayout>();
}
std::unique_ptr<WindowLayout> createFreeformLayout() {
  return std::make_unique<FreeformLayout>();
}
std::unique_ptr<WindowLayout> createScrollingLayout() {
  return std::make_unique<ScrollingLayout>();
}


Json defaultWindowAnimation() {
  return {{"duration", -1},
          {"enterOffset", 8},
          {"focusOpacity", 1.0},
          {"exitScale", 0.96},
          {"easing", "outCubic"}};
}

// Critically damped springs: no overshoot, and interrupted motions continue
// with their velocity. windowMovement is every displacement: layout moves and
// resizes and workspace switches share it.
Json defaultWindowMotion() {
  const auto spring = [](double stiffness) {
    return Json{{"stiffness", stiffness}, {"dampingRatio", 1.0}};
  };
  return {{"openScale", 0.92},
          {"closeDuration", 150},
          {"windowOpen", spring(800)},
          {"windowMovement", spring(800)}};
}

Json commonActions() {
  const auto window = windowLayoutIntegerParameter(1, 2147483647, 1);
  const auto workspace = windowLayoutIntegerParameter(0, 2147483647);
  const auto delta = windowLayoutIntegerParameter(-32768, 32768);
  const auto size = windowLayoutIntegerParameter(1, 32768, 1);
  auto direction = windowLayoutIntegerParameter(-1, 1, 1);
  direction["enum"] = Json::array({-1, 1});
  return {
      {"focus-direction", windowLayoutAction("Directional focus", {{"workspace", workspace},
          {"dx", windowLayoutIntegerParameter(-1, 1)}, {"dy", windowLayoutIntegerParameter(-1, 1)}}, false, true)},
      {"swap", windowLayoutAction("Swap windows", {{"window", window}, {"target", window}})},
      {"reorder", windowLayoutAction("Reorder window", {{"window", window}, {"direction", direction}})},
      {"resize-width", windowLayoutAction("Resize width", {{"window", window}, {"width", size}})},
      {"resize-height", windowLayoutAction("Resize height", {{"window", window}, {"height", size}})},
      {"move-by", windowLayoutAction("Move window", {{"window", window}, {"dx", delta}, {"dy", delta}}, true)},
      {"center", windowLayoutAction("Center window", {{"window", window}}, true)}};
}
Json tilingActions() {
  auto actions = commonActions();
  const auto window = windowLayoutIntegerParameter(1, 2147483647, 1);
  actions["group-direction"] = windowLayoutAction("Group in direction",
      {{"window", window}, {"workspace", windowLayoutIntegerParameter(0, 2147483647)},
       {"dx", windowLayoutIntegerParameter(-1, 1)}, {"dy", windowLayoutIntegerParameter(-1, 1)}}, false, true);
  actions["group-with"] = windowLayoutAction("Group with window", {{"window", window}, {"target", window}});
  actions["expel"] = windowLayoutAction("Expel from group", {{"window", window}});
  actions["insert-beside"] = windowLayoutAction("Insert beside window", {{"window", window}, {"target", window},
      {"after", Json::object({{"type", "boolean"}, {"default", true}})}});
  return actions;
}

WindowTemplate tilingTemplate() {
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
  WindowTemplate value{"tiling", &createTilingLayout, false, false, true,
                       schema, windowTemplateDefaults(schema), actions,
                       defaultWindowAnimation()};
  value.layoutMode = "tiling";
  value.variantPreference = tilingModePreference;
  value.variant = "fill";
  value.sharesDesktopGap = true;
  value.motion = defaultWindowMotion();
  return value;
}

WindowTemplate stackingTemplate() {
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
  WindowTemplate value{"stacking", &createFreeformLayout, true, true, false,
                       schema, windowTemplateDefaults(schema), actions,
                       defaultWindowAnimation()};
  value.layoutMode = "stacking";
  value.motion = defaultWindowMotion();
  return value;
}
// niri-like columns: the "normal" counterpart of tiling's fill-the-screen tree.
WindowTemplate scrollingTemplate() {
  static const Json schema{
      {"gap", Json::object({{"type", "integer"},
                           {"default", 12},
                           {"minimum", 0},
                           {"maximum", 64},
                           {"label", "Window gaps"}})},
      {"columnWidth", Json::object({{"type", "integer"},
                                   {"default", 50},
                                   {"minimum", 20},
                                   {"maximum", 100},
                                   {"label", "New column width (% of the screen)"}})},
      {"centerFocusedColumn", Json::object({{"type", "string"},
                                           {"default", "never"},
                                           {"enum", Json{"never", "always"}},
                                           {"label", "Center the focused column"},
                                           {"description", "Never scrolls only as far as needed to show the focused column."},
                                           {"control", "select"}})}};
  auto actions = tilingActions();
  actions.erase("move-by");
  static const auto scrollingActions = actions;
  WindowTemplate value{"scrolling", &createScrollingLayout, false, false, false,
                       schema, windowTemplateDefaults(schema), scrollingActions,
                       defaultWindowAnimation()};
  value.layoutMode = "tiling";
  value.variantPreference = tilingModePreference;
  value.variant = "scroll";
  value.sharesDesktopGap = true;
  value.motion = defaultWindowMotion();
  return value;
}
} // namespace


void registerBuiltinWindowTemplates() {
  registerWindowTemplate(tilingTemplate(), true);
  registerWindowTemplate(stackingTemplate());
  registerWindowTemplate(scrollingTemplate());
}

} // namespace LunaDash
