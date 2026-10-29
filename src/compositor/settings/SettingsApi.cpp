#include "compositor/settings/SettingsApi.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/PluginCatalog.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/settings/SettingsTarget.hpp"
#include "core/text/Text.hpp"

namespace LunaDash {
Json settingsApiTargets(const Json &extensions, const Json &layout) {
  Json targets = Json::array();
  for (const auto &item : jsonArray(extensions, "targets")) {
    // The layout template owns these settings; don't offer a second gap editor.
    if (jsonString(item, "id") == "window-layout") continue;
    targets.push_back(Settings::target(
        "builtin:" + jsonString(item, "id"), jsonString(item, "name"),
        "builtin", jsonString(item, "category"), jsonObject(item, "settings"),
        jsonObject(item, "builtinSettings")));
  }
  for (const auto &item : jsonArray(extensions, "installed")) {
    if (!jsonString(item, "error").empty()) continue;
    targets.push_back(Settings::target(
        "plugin:" + jsonString(item, "id") + ":" + jsonString(item, "target"),
        jsonString(item, "name") + " / " + jsonString(item, "target"),
        jsonString(item, "type"), jsonString(item, "target"),
        jsonObject(item, "settingsSchema"), jsonObject(item, "settings")));
  }
  if (layout.is_object() && !layout.empty()) targets.push_back(layout);
  return targets;
}

bool updateSettingsApi(
    PluginManager &plugins, const Json &layout,
    const Json &request,
    const std::function<bool(const Json &, std::string *)> &setLayout,
    std::string *error) {
  const auto fail = [error](const std::string &message) {
    if (error) *error = message;
    return false;
  };
  if (!request.is_object()) return fail("Invalid settings API request.");
  for (auto it = request.begin(); it != request.end(); ++it)
    if (it.key() != "target" && it.key() != "revision" && it.key() != "changes")
      return fail("Unknown settings API request field: " + it.key());
  const auto id = jsonString(request, "target");
  if (id.empty() || id.size() > 512) return fail("Invalid settings target.");
  plugins.refresh();
  const auto targets = settingsApiTargets(plugins.snapshot(), layout);
  Json descriptor = Json::object();
  for (const auto &entry : targets)
    if (jsonString(entry, "id") == id) descriptor = entry;
  if (!Settings::checkPatch(descriptor, request, error)) return false;
  const auto changes = jsonObject(request, "changes");
  if (id == jsonString(layout, "id")) return setLayout(changes, error);
  const auto parts = splitText(id, ':');
  std::string configError;
  auto document = readExtensionConfiguration(&configError);
  if (!configError.empty()) return fail(configError);
  if (parts.empty()) return fail("Invalid settings target.");
  if (parts.size() == 2 && parts[0] == "builtin") {
    auto builtins = jsonObject(document, "builtins");
    builtins[parts[1]] = Settings::merged(jsonObject(builtins, parts[1]), changes);
    document["builtins"] = builtins;
  } else if (parts[0] == "plugin" && parts.size() == 3) {
    bool found = false;
    for (const auto &plugin : discoverPlugins()) {
      if (plugin.id != parts[1] || plugin.target != parts[2])
        continue;
      if (!plugin.error.empty())
        return fail(plugin.error);
      auto entries = jsonObject(document, "plugins");
      auto package = jsonObject(entries, plugin.id);
      package["enabled"] = jsonBool(package, "enabled", true);
      auto targetEntries = jsonObject(package, "targets");
      auto targetConfig = jsonObject(targetEntries, plugin.target);
      targetConfig["enabled"] = jsonBool(targetConfig, "enabled", plugin.enabled);
      targetConfig["mode"] = jsonString(targetConfig, "mode", plugin.mode);
      targetConfig["settings"] = Settings::merged(plugin.settings, changes);
      targetEntries[plugin.target] = targetConfig;
      package.erase("mode");
      package.erase("settings");
      package["targets"] = targetEntries;
      entries[plugin.id] = package;
      document["plugins"] = entries;
      found = true;
      break;
    }
    if (!found)
      return fail("Plugin target is no longer installed.");
  } else return fail("Unknown settings target.");
  if (!saveExtensionConfiguration(jsonCompact(document), error)) return false;
  plugins.refresh();
  return true;
}
} // namespace LunaDash
