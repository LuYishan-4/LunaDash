#include "compositor/settings/SettingsApi.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/PluginCatalog.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/settings/SettingsTarget.hpp"
#include "core/text/Text.hpp"
#include "shell/modules/ShellModules.hpp"
#include "shell/modules/ShellModuleSchema.hpp"

namespace LunaDash {
Json settingsApiTargets(const Json &extensions, const Json &modules,
                        const Json &layout) {
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
  const auto values = jsonObject(jsonObject(modules, "document"), "modules");
  for (const auto &item : jsonArray(modules, "descriptors")) {
    const auto id = jsonString(item, "id");
    const auto sections = jsonObject(item, "sections");
    const auto module = jsonObject(values, id);
    for (auto it = sections.begin(); it != sections.end(); ++it) {
      const auto sectionValues =
          it.key() == "module" ? Json{{"enabled", jsonValue(module, "enabled")}}
                               : jsonObject(module, it.key());
      targets.push_back(Settings::target(
          "module:" + id + ":" + it.key(),
          jsonString(item, "name") + " / " + it.key(), "module",
          jsonString(item, "type"), jsonAsObject(it.value()), sectionValues));
    }
  }
  if (layout.is_object() && !layout.empty()) targets.push_back(layout);
  return targets;
}

bool updateSettingsApi(
    PluginManager &plugins, ShellModules &modules, const Json &layout,
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
  auto moduleState = modules.snapshot();
  // Read current disk contents, not a potentially debounced watcher snapshot.
  if (id.starts_with("module:")) {
    const auto path = jsonString(moduleState, "path");
    const auto text = path.empty() ? std::nullopt : readFile(path, 16384);
    ShellModuleJson document;
    std::string nativeError = "Could not read module configuration.";
    if (!text || !validateModuleDocument(*text, &document, &nativeError)) {
      if (error)
        *error = nativeError;
      return fail("Module file changed or is unreadable; reload before editing.");
    }
    moduleState["document"] = document;
  }
  const auto targets = settingsApiTargets(plugins.snapshot(), moduleState, layout);
  Json descriptor = Json::object();
  for (const auto &entry : targets)
    if (jsonString(entry, "id") == id) descriptor = entry;
  if (!Settings::checkPatch(descriptor, request, error)) return false;
  const auto changes = jsonObject(request, "changes");
  if (id == jsonString(layout, "id")) return setLayout(changes, error);
  const auto parts = splitText(id, ':');
  if (parts.size() == 3 && parts[0] == "module") {
    auto document = jsonObject(moduleState, "document");
    auto entries = jsonObject(document, "modules");
    auto module = jsonObject(entries, parts[1]);
    if (parts[2] == "module") module = Settings::merged(module, changes);
    else module[parts[2]] = Settings::merged(jsonObject(module, parts[2]), changes);
    entries[parts[1]] = module;
    document["modules"] = entries;
    std::string nativeError;
    if (!modules.apply(jsonCompact(document), &nativeError)) {
      if (error)
        *error = nativeError;
      return false;
    }
    return true;
  }
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
