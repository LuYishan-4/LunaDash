#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "config/plugins/PluginCatalog.hpp"
#include "core/filesystem/FileIo.hpp"
#include <filesystem>
#include <glib.h>
#include <initializer_list>
#include <string_view>
#include <algorithm>
#include <map>
#include <set>

namespace LunaDash {
namespace {
bool fail(std::string *error, const std::string &message) {
  if (error)
    *error = message;
  return false;
}
bool allowed(const Json &object,
             std::initializer_list<std::string_view> keys) {
  for (auto it = object.begin(); it != object.end(); ++it)
    if (std::find(keys.begin(), keys.end(), it.key()) == keys.end())
      return false;
  return true;
}
bool compositionMode(const std::string &mode) {
  return mode == "replace" || mode == "augment";
}
Json effectiveTargetConfig(const PluginDescriptor &descriptor,
                                  const Json &package) {
  const auto targets = jsonObject(package, "targets");
  if (jsonContains(targets, descriptor.target))
    return jsonObject(targets, descriptor.target);
  if (!jsonValue(descriptor.manifest, "targets").is_array())
    return jsonAsObject(package);
  return Json::object();
}
bool effectiveEnabled(const PluginDescriptor &descriptor,
                      const Json &package) {
  const bool packageEnabled = jsonBool(package, "enabled",
      jsonBool(descriptor.manifest, "enabledByDefault", false));
  const auto target = effectiveTargetConfig(descriptor, package);
  return packageEnabled && jsonBool(target, "enabled", true);
}
std::string effectiveMode(const PluginDescriptor &descriptor,
                      const Json &package) {
  return jsonString(effectiveTargetConfig(descriptor, package), "mode",
                    descriptor.mode);
}
} // namespace

std::string extensionConfigurationPath() {
  return std::string(g_get_user_config_dir()) +
         "/LuDash/extensions.json";
}

Json readExtensionConfiguration(std::string *error) {
  const Json empty{{"schemaVersion", 1},
                          {"builtins", Json::object()},
                          {"plugins", Json::object()}};
  const auto path = extensionConfigurationPath();
  std::error_code existsError;
  if (!std::filesystem::exists(path, existsError))
    return empty;
  const auto contents = readFile(path, 24576);
  if (!contents) {
    fail(error, "Could not read extensions.json (limit 24 KiB).");
    return empty;
  }
  const auto object = parseJson(*contents);
  if (!object.is_object() || jsonInt(object, "schemaVersion") != 1 ||
      !jsonValue(object, "builtins").is_object() ||
      !jsonValue(object, "plugins").is_object()) {
    fail(error, "Invalid extensions.json; built-in defaults are in use.");
    return empty;
  }
  return object;
}

Json configuredBuiltinSettings(const std::string &target) {
  return configuredBuiltinSettings(target, readExtensionConfiguration());
}

Json configuredBuiltinSettings(const std::string &target,
                                      const Json &document) {
  const auto schema = jsonObject(extensionTarget(target), "settings");
  auto result = extensionDefaults(schema);
  const auto changes = jsonObject(jsonObject(document, "builtins"), target);
  if (validateExtensionSettings(schema, changes, nullptr))
    for (auto it = changes.begin(); it != changes.end(); ++it)
      result[it.key()] = it.value();
  return result;
}

bool saveExtensionConfiguration(const std::string &json, std::string *error) {
  const auto root = parseJson(json);
  if (json.size() > 24576 || !root.is_object() ||
      jsonInt(root, "schemaVersion") != 1 ||
      !jsonValue(root, "builtins").is_object() || !jsonValue(root, "plugins").is_object() ||
      !allowed(root, {"schemaVersion", "builtins", "plugins"}))
    return fail(
        error,
        "Expected schemaVersion 1, builtins and plugins (24 KiB limit).");

  const auto builtins = jsonObject(root, "builtins");
  for (auto it = builtins.begin(); it != builtins.end(); ++it) {
    const auto target = extensionTarget(it.key());
    if (target.empty() || !it.value().is_object())
      return fail(error, "Unknown built-in target: " + it.key());
    if (!validateExtensionSettings(jsonObject(target, "settings"),
                                   it.value(), error))
      return false;
  }

  const auto catalog = discoverPlugins();
  const auto plugins = jsonObject(root, "plugins");

  for (auto it = plugins.begin(); it != plugins.end(); ++it) {
    if (!it.value().is_object())
      return fail(error, "Plugin configuration must be an object.");
    const auto &package = it.value();
    const bool multi = jsonValue(package, "targets").is_object();
    if (multi) {
      if (!allowed(package, {"enabled", "targets"}) ||
          !jsonValue(package, "enabled").is_boolean())
        return fail(error, "Multi-target plugin configuration expects enabled and targets: " + it.key());
      const auto targets = jsonObject(package, "targets");
      for (auto targetIt = targets.begin(); targetIt != targets.end(); ++targetIt) {
        if (!targetIt.value().is_object())
          return fail(error, "Plugin target configuration must be an object.");
        const auto &config = targetIt.value();
        if (!allowed(config, {"enabled", "mode", "settings"}) ||
            !jsonValue(config, "enabled").is_boolean() ||
            !jsonValue(config, "settings").is_object() ||
            !compositionMode(jsonString(config, "mode")))
          return fail(error, "Expected enabled, mode and settings for " +
                                 it.key() + "/" + targetIt.key());
        const auto found = std::find_if(
            catalog.cbegin(), catalog.cend(), [&](const auto &plugin) {
              return plugin.id == it.key() && plugin.target == targetIt.key();
            });
        if (found == catalog.cend()) {
          if (jsonBool(package, "enabled") &&
              jsonBool(config, "enabled"))
            return fail(error, "Plugin target is not installed: " +
                                   it.key() + "/" + targetIt.key());
          continue;
        }
        const auto descriptor =
            readPluginMetadataTargets(found->metadataPath, false);
        const auto targetDescriptor = std::find_if(
            descriptor.cbegin(), descriptor.cend(), [&](const auto &plugin) {
              return plugin.target == targetIt.key();
            });
        if (targetDescriptor == descriptor.cend() ||
            !targetDescriptor->error.empty())
          return fail(error, targetDescriptor == descriptor.cend()
                                 ? "Plugin target disappeared during validation."
                                 : targetDescriptor->error);
        if (!validateExtensionSettings(targetDescriptor->settingsSchema,
                                       jsonObject(config, "settings"),
                                       error))
          return false;
      }
      continue;
    }

    if (!allowed(package, {"enabled", "mode", "settings"}) ||
        !jsonValue(package, "enabled").is_boolean() ||
        !jsonValue(package, "settings").is_object() ||
        !compositionMode(jsonString(package, "mode")))
      return fail(error,
                  "Expected enabled, mode (replace/augment) and settings: " +
                      it.key());

    const auto matching = std::count_if(
        catalog.cbegin(), catalog.cend(),
        [&](const auto &plugin) { return plugin.id == it.key(); });
    if (matching == 0) {
      if (jsonBool(package, "enabled"))
        return fail(error, "Plugin is not installed: " + it.key());
      continue;
    }
    if (matching != 1)
      return fail(error, "Multi-target plugin requires a targets object: " +
                             it.key());
    const auto found = std::find_if(catalog.cbegin(), catalog.cend(),
                                    [&](const auto &plugin) {
                                      return plugin.id == it.key();
                                    });
    const auto descriptor = readPluginMetadata(found->metadataPath, false);
    if (!descriptor.error.empty() && jsonBool(package, "enabled"))
      return fail(error, descriptor.error);
    if (descriptor.error.empty() &&
        !validateExtensionSettings(descriptor.settingsSchema,
                                   jsonObject(package, "settings"), error))
      return false;
  }

  std::map<std::string, std::string> selected;
  std::map<std::string, std::string> replacements;
  std::set<std::string> visitedPackages;
  for (const auto &installed : catalog) {
    if (visitedPackages.contains(installed.id + "@" + installed.target))
      continue;
    visitedPackages.insert(installed.id + "@" + installed.target);
    const auto descriptors =
        readPluginMetadataTargets(installed.metadataPath, false);
    const auto descriptor = std::find_if(
        descriptors.cbegin(), descriptors.cend(), [&](const auto &candidate) {
          return candidate.target == installed.target;
        });
    if (descriptor == descriptors.cend() || !descriptor->error.empty())
      continue;

    const auto package = jsonObject(plugins, descriptor->id);
    if (!effectiveEnabled(*descriptor, package))
      continue;

    const auto targetSpec = extensionTarget(descriptor->target);
    const auto owner = descriptor->id + "@" + descriptor->target;
    if (jsonString(targetSpec, "selection", "single") == "single") {
      const auto selectedOwner = selected.find(descriptor->target);
      if (selectedOwner != selected.end() && selectedOwner->second != owner)
        return fail(error, "Only one plugin can be enabled for: " +
                               descriptor->target);
      selected[descriptor->target] = owner;
    }

    if (effectiveMode(*descriptor, package) == "replace") {
      const auto replacementOwner = replacements.find(descriptor->target);
      if (replacementOwner != replacements.end() &&
          replacementOwner->second != owner)
        return fail(error, "Only one replacement can be enabled for: " +
                               descriptor->target);
      replacements[descriptor->target] = owner;
    }
  }

  const auto path = extensionConfigurationPath();
  std::error_code linkError;
  if (std::filesystem::is_symlink(path, linkError))
    return fail(error, "Refusing to replace a symlinked extensions.json.");
  if (!writeFileAtomically(path, jsonCompact(root), nullptr,
                           std::filesystem::perms::owner_read |
                               std::filesystem::perms::owner_write))
    return fail(error, "Could not save extensions.json atomically.");
  return true;
}
} // namespace LunaDash
