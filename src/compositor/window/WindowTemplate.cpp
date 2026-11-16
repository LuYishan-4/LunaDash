#include "compositor/window/WindowTemplate.hpp"

#include "core/settings/SettingsSchema.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <mutex>

namespace LunaDash {
namespace {

struct WindowTemplateRegistry {
  // A deque keeps references stable: the compositor holds the active template.
  std::deque<WindowTemplate> templates;
  const WindowTemplate *defaultTemplate = nullptr;
};

WindowTemplateRegistry &registryStorage() {
  static WindowTemplateRegistry storage;
  return storage;
}

// Lookups see the built-in templates even before anything else registered.
WindowTemplateRegistry &registry() {
  static std::once_flag builtins;
  std::call_once(builtins, [] { registerBuiltinWindowTemplates(); });
  return registryStorage();
}

} // namespace

bool registerWindowTemplate(WindowTemplate windowTemplate, bool makeDefault,
                            std::string *error) {
  auto &storage = registryStorage();
  if (windowTemplate.key.empty() || !windowTemplate.createLayout) {
    if (error)
      *error = "A window template needs a key and a layout factory.";
    return false;
  }
  if (std::any_of(storage.templates.begin(), storage.templates.end(),
                  [&windowTemplate](const WindowTemplate &existing) {
                    return existing.key == windowTemplate.key;
                  })) {
    if (error)
      *error = "Window template " + windowTemplate.key + " is already registered.";
    return false;
  }
  storage.templates.push_back(std::move(windowTemplate));
  if (makeDefault && !storage.defaultTemplate)
    storage.defaultTemplate = &storage.templates.back();
  return true;
}

std::vector<WindowTemplate> windowTemplates() {
  const auto &storage = registry();
  return {storage.templates.begin(), storage.templates.end()};
}

const WindowTemplate &defaultWindowTemplate() {
  const auto &storage = registry();
  if (storage.defaultTemplate)
    return *storage.defaultTemplate;
  if (!storage.templates.empty())
    return storage.templates.front();
  static const WindowTemplate empty;
  return empty;
}

const WindowTemplate &windowTemplateForKey(std::string_view key) {
  for (const auto &candidate : registry().templates)
    if (candidate.key == key)
      return candidate;
  return defaultWindowTemplate();
}

const WindowTemplate &resolveWindowTemplate(std::string_view key,
                                            const Json &preferences) {
  const auto &requested = windowTemplateForKey(key);
  if (requested.variantPreference.empty())
    return requested;
  const auto variant =
      preferences.value(requested.variantPreference, requested.variant);
  for (const auto &candidate : registry().templates)
    if (candidate.variantPreference == requested.variantPreference &&
        candidate.variant == variant)
      return candidate;
  return requested;
}

std::vector<std::string> windowTemplateVariants(std::string_view variantPreference) {
  std::vector<std::string> variants;
  for (const auto &candidate : registry().templates)
    if (!variantPreference.empty() &&
        candidate.variantPreference == variantPreference)
      variants.push_back(candidate.variant);
  return variants;
}

Json windowLayoutIntegerParameter(int minimum, int maximum, int value) {
  return {{"type", "integer"}, {"minimum", minimum}, {"maximum", maximum},
          {"default", value}};
}

Json windowLayoutAction(const std::string &label, Json parameters,
                        bool requiresArea, bool cardinalDirection) {
  Json required = Json::array();
  for (auto it = parameters.begin(); it != parameters.end(); ++it)
    required.push_back(it.key());
  return {{"label", label}, {"parameters", parameters}, {"required", required},
          {"requiresArea", requiresArea}, {"cardinalDirection", cardinalDirection}};
}

Json windowTemplateDefaults(const Json &schema) {
  Json result = Json::object();
  for (auto it = schema.begin(); it != schema.end(); ++it)
    result[it.key()] = jsonValue(it.value(), "default");
  return result;
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
        {"x", windowLayoutIntegerParameter(-32768, 32768)}, {"y", windowLayoutIntegerParameter(-32768, 32768)},
        {"width", windowLayoutIntegerParameter(1, 32768, 1)}, {"height", windowLayoutIntegerParameter(1, 32768, 1)}};
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
