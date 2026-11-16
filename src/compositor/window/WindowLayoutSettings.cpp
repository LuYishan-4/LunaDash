#include "compositor/plugins/PluginManager.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/desktop/PreferenceKeys.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "core/json/JsonAccess.hpp"
#include "core/log/Log.hpp"

#include <string>

namespace LunaDash {

std::string WaylandCompositor::selectedWindowTemplateKey() const {
  // A window-layout plugin wins over the default template; a template with
  // variants follows the variant its preference selects.
  return resolveWindowTemplate(pluginManager_->windowTemplateKey(),
                               desktopPreferences())
      .key;
}

Json WaylandCompositor::currentWindowLayoutSettings() const {
  if (!windowTemplate_)
    return Json::object();

  auto values = windowTemplate_->layoutSettingsDefaults;

  // Preserve legacy gap preferences until the user edits the new template API.
  if (windowTemplate_->sharesDesktopGap) {
    const auto preferences = desktopPreferences();
    if (preferences.contains(Preference::gap.name))
      values["gap"] = jsonInt(preferences, Preference::gap.name);
    const int legacyGap =
        jsonInt(configuredBuiltinSettings("window-layout"), "gap", -1);
    if (legacyGap >= 0)
      values["gap"] = legacyGap;
  }

  std::string storageError;
  const auto stored = loadWindowLayoutSettings(
      windowTemplate_->key, windowTemplate_->layoutSettingsSchema, &storageError);
  if (!storageError.empty())
    logWarning("Could not load window layout settings: %s",
               storageError.c_str());
  const auto storedValues = jsonAsObject(stored);
  for (auto it = storedValues.begin(); it != storedValues.end(); ++it)
    values[it.key()] = it.value();

  if (!validateWindowLayoutSettings(*windowTemplate_, values, nullptr))
    return windowTemplate_->layoutSettingsDefaults;
  return values;
}

bool WaylandCompositor::updateWindowLayoutSettings(const Json &changes,
                                                   std::string *error) {
  if (!windowTemplate_) {
    if (error)
      *error = "No window layout template is active.";
    return false;
  }
  if (!validateWindowLayoutSettings(*windowTemplate_, changes, error))
    return false;

  auto values = currentWindowLayoutSettings();
  for (auto it = changes.begin(); it != changes.end(); ++it)
    values[it.key()] = it.value();
  if (!validateWindowLayoutSettings(*windowTemplate_, values, error))
    return false;

  std::string storageError;
  if (!saveWindowLayoutSettings(windowTemplate_->key, values, &storageError)) {
    if (error)
      *error = storageError;
    return false;
  }
  return true;
}

bool WaylandCompositor::performLayoutAction(const std::string &action,
                                            const Json &payload) {
  return windowTemplate_ &&
         performWindowLayoutAction(*windowLayout_, *windowTemplate_, action,
                                   payload);
}

} // namespace LunaDash
