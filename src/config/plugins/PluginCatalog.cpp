#include "config/plugins/PluginCatalog.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/crypto/Sha256.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/text/Url.hpp"
#include "core/settings/SettingsSchema.hpp"
#include "core/text/Text.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <glib.h>
#include <set>
#include <string_view>

namespace LunaDash {
namespace {
std::string localized(const Json &object, const std::string &key,
                      const std::string &locale) {
  return jsonString(object, key + "[" + locale + "]", jsonString(object, key));
}

bool safeLeaf(const std::string &value) {
  return !value.empty() && value.find('/') == std::string::npos &&
         value.find('\\') == std::string::npos && value != "." && value != "..";
}

std::string canonicalPath(const std::filesystem::path &path) {
  std::error_code error;
  const auto result = std::filesystem::canonical(path, error);
  return error ? std::string() : result.string();
}

std::string canonicalChild(const std::filesystem::path &directory,
                           const std::string &leaf) {
  if (!safeLeaf(leaf))
    return {};
  const auto root = canonicalPath(directory);
  const auto child = canonicalPath(directory / leaf);
  if (root.empty() || child.empty() ||
      std::filesystem::path(child).parent_path().string() != root)
    return {};
  return child;
}

bool packagedImageIcon(const std::string &value) {
  const auto lower = asciiLower(value);
  for (const auto *suffix : {".png", ".jpg", ".jpeg", ".webp", ".svg"})
    if (lower.ends_with(suffix))
      return true;
  return false;
}

bool validPluginIdentity(const std::string &id) {
  const auto alphanumeric = [](unsigned char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9');
  };
  return id.size() >= 2 && alphanumeric(static_cast<unsigned char>(id.front())) &&
         std::all_of(id.begin() + 1, id.end(), [&](unsigned char character) {
           return alphanumeric(character) || character == '.' ||
                  character == '_' || character == '-';
         });
}

bool compositionMode(const std::string &mode) {
  return mode == "replace" || mode == "augment";
}

std::string systemLocaleName() {
  for (const char *name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
    const char *value = std::getenv(name);
    if (!value || !*value)
      continue;
    std::string locale(value);
    locale = locale.substr(0, locale.find_first_of(".@"));
    if (locale == "C" || locale == "POSIX")
      return "C";
    return locale;
  }
  return "C";
}

Json targetConfiguration(const PluginDescriptor &plugin,
                         const Json &packageConfig) {
  const auto targets = jsonObject(packageConfig, "targets");
  if (jsonContains(targets, plugin.target))
    return jsonObject(targets, plugin.target);
  if (jsonValue(plugin.manifest, "targets").is_array())
    return Json::object();
  return jsonAsObject(packageConfig);
}

PluginDescriptor errorDescriptor(const std::string &path, const std::string &message) {
  PluginDescriptor result;
  result.metadataPath = path;
  result.error = message;
  return result;
}
} // namespace

std::string pluginInstanceId(const PluginDescriptor &plugin) {
  return plugin.id + "@" + plugin.target;
}

std::string pluginStoreFingerprint(const Json &storeItem) {
  Json hashes = Json::object();
  for (const auto &file : jsonArray(jsonObject(storeItem, "install"), "files")) {
    if (file.is_object())
      hashes[jsonString(file, "path")] = jsonValue(file, "sha256");
  }
  if (hashes.empty())
    return {};
  const Json package{{"id", jsonValue(storeItem, "id")},
                     {"version", jsonValue(storeItem, "version")},
                     {"files", hashes}};
  return sha256Hex(jsonCompact(package));
}

bool pluginMatchesStore(const PluginDescriptor &plugin,
                        const Json &storeItem) {
  if (plugin.version != jsonString(storeItem, "version"))
    return false;
  const auto fingerprint = pluginStoreFingerprint(storeItem);
  if (fingerprint.empty())
    return true;
  const auto receipt = readFile(
      std::filesystem::absolute(plugin.metadataPath).parent_path() /
          ".lunadash-store.json",
      4096);
  if (!receipt)
    return false;
  return jsonString(parseJson(*receipt), "fingerprint") == fingerprint;
}

std::vector<PluginDescriptor>
readPluginMetadataTargets(const std::string &path, bool applyConfiguration) {
  const auto contents = readFile(path, 65536);
  if (!contents)
    return {errorDescriptor(path, "Unreadable or oversized metadata")};

  const auto &metadataBytes = *contents;
  const auto metadata = parseJson(metadataBytes);
  if (!metadata.is_object())
    return {errorDescriptor(path, "Invalid JSON metadata")};

  const int schemaVersion = jsonInt(metadata, "schemaVersion", 1);
  const auto preferences = desktopPreferences();
  const auto locale =
      jsonString(preferences, "language", systemLocaleName());
  const auto directory = std::filesystem::absolute(path).parent_path();

  PluginDescriptor base;
  base.metadataPath = path;
  base.manifest = metadata;
  base.schemaVersion = schemaVersion;

  if (jsonContains(metadata, "KPlugin")) {
    const auto info = jsonObject(metadata, "KPlugin");
    const auto api = jsonObject(metadata, "LuDash");
    base.id = jsonString(info, "Id");
    base.version = jsonString(info, "Version");
    base.name = localized(info, "Name", locale);
    base.description = localized(info, "Description", locale);
    const auto authors = jsonArray(info, "Authors");
    base.author = authors.empty() ? std::string()
                                  : jsonString(authors.front(), "Name");
    base.icon = jsonString(info, "Icon", "applications-system");
    base.type = "effect";
    base.target = "window-animation";
    base.mode = "augment";
    base.implementation = metadata;
    base.libraryPath = canonicalChild(directory, jsonString(api, "Library"));
    base.error = "Legacy native effect: rebuild with LunaDash Plugin SDK 2";
    return {base};
  }

  if (schemaVersion != 1 && schemaVersion != 2)
    return {errorDescriptor(path, "Unsupported plugin manifest schema")};

  base.id = jsonString(metadata, "id");
  base.version = jsonString(metadata, "version");
  base.name = localized(metadata, "name", locale);
  base.description = localized(metadata, "description", locale);
  const auto author = jsonValue(metadata, "author");
  base.author = author.is_object() ? jsonString(author, "name")
                                   : jsonAsString(author);
  base.icon = jsonString(metadata, "icon", "applications-system");

  const auto tagsValue = jsonValue(metadata, "tags");
  if (tagsValue.is_array()) {
    std::set<std::string> seenTags;
    for (const auto &value : tagsValue) {
      const auto tag = trimmed(jsonAsString(value));
      if (tag.empty() || tag.size() > 32 || seenTags.contains(tag))
        continue;
      seenTags.insert(tag);
      base.tags.push_back(tag);
    }
  }

  if (!validPluginIdentity(base.id) || base.name.empty() ||
      base.version.empty()) {
    base.error = "Missing or invalid plugin identity";
    return {base};
  }

  if (schemaVersion == 2) {
    const auto receiptPath = canonicalChild(directory, ".lunadash-sdk.json");
    const auto receipt =
        receiptPath.empty() ? std::nullopt : readFile(receiptPath, 4096);
    if (!receipt ||
        jsonString(parseJson(*receipt), "metadataSha256") !=
            sha256Hex(metadataBytes)) {
      base.error = "Missing or stale LunaDash CMake SDK build receipt; rebuild the plugin";
      return {base};
    }
    const auto sdk = jsonObject(metadata, "sdk");
    if (jsonString(sdk, "name") != "LunaDash" ||
        jsonInt(sdk, "apiVersion") != 2) {
      base.error = "Invalid LunaDash Plugin SDK declaration";
      return {base};
    }
    if (!safeLeaf(base.icon)) {
      base.error = "Plugin icon must be a theme name or local image filename";
      return {base};
    }
    if (packagedImageIcon(base.icon)) {
      const auto iconPath = canonicalChild(directory, base.icon);
      if (iconPath.empty()) {
        base.error = "Plugin icon is missing or escapes the plugin directory";
        return {base};
      }
      base.icon = fileUrl(iconPath);
    }
  }

  Json implementations = Json::array();
  if (schemaVersion == 2 && jsonValue(metadata, "targets").is_array()) {
    implementations = jsonArray(metadata, "targets");
    if (implementations.empty() || implementations.size() > 16) {
      base.error = "Plugin targets must contain between 1 and 16 entries";
      return {base};
    }
  } else {
    implementations.push_back(metadata);
  }

  const auto allConfig = applyConfiguration ? readExtensionConfiguration() : Json::object();
  const auto packageConfig =
      jsonObject(jsonObject(allConfig, "plugins"), base.id);
  const bool packageDefault = jsonBool(metadata, "enabledByDefault", false);
  const bool packageEnabled =
      applyConfiguration
          ? jsonBool(packageConfig, "enabled", packageDefault)
          : packageDefault;

  std::vector<PluginDescriptor> results;
  std::set<std::string> targetIds;
  for (const auto &value : implementations) {
    if (!value.is_object()) {
      auto invalid = base;
      invalid.error = "Plugin target entries must be objects";
      return {invalid};
    }
    const auto &implementation = value;
    PluginDescriptor result = base;
    result.implementation = implementation;
    result.type = asciiLower(jsonString(implementation, "type"));
    result.target = jsonString(
        implementation, "target",
        jsonString(implementation, "id",
                   jsonString(metadata, "target")));
    result.mode = jsonString(implementation, "mode", "augment");

    if (result.target.empty() || targetIds.contains(result.target)) {
      result.error = "Plugin targets must have unique non-empty target ids";
      return {result};
    }
    targetIds.insert(result.target);

    const auto target = extensionTarget(result.target);
    if (schemaVersion == 2 &&
        (target.empty() ||
         !Settings::Detail::arrayContains(jsonArray(target, "types"),
                                          Json(result.type)) ||
         !compositionMode(result.mode))) {
      result.error = "Invalid SDK target, type or composition mode";
      return {result};
    }

    result.settingsSchema = jsonObject(implementation, "settings");
    result.settings = extensionDefaults(result.settingsSchema);
    if (schemaVersion == 2 &&
        (!jsonValue(implementation, "settings").is_object() ||
         !Settings::validatePluginSchema(result.settingsSchema, &result.error) ||
         !Settings::validateValues(result.settingsSchema, result.settings,
                                   &result.error)))
      return {result};

    const auto windowTemplate =
        jsonString(implementation, "windowTemplate", "tiling");
    if ((jsonContains(implementation, "windowTemplate") &&
         (result.type != "effect" || result.target != "window-layout")) ||
        jsonContains(implementation, "layoutMode") ||
        (windowTemplate != "tiling" && windowTemplate != "stacking") ||
        (windowTemplate == "stacking" && result.mode != "replace")) {
      result.error =
          "windowTemplate stacking requires a window-layout replacement";
      return {result};
    }

    // Plugins are native effects; the Quickshell and OpenGL plugins of the
    // former QML shell are no longer loaded.
    if (result.type == "effect") {
      result.libraryPath =
          canonicalChild(directory, jsonString(implementation, "entry"));
      if (result.libraryPath.empty()) {
        result.error = "Effect plugin entry must be a library inside its directory";
        return {result};
      }
      if (schemaVersion != 2) {
        result.error = "Legacy native effect: rebuild with LunaDash Plugin SDK 2";
        return {result};
      }
    } else {
      result.error = "Plugin type must be effect";
      return {result};
    }

    const auto config = targetConfiguration(result, packageConfig);
    const bool targetEnabled =
        jsonBool(config, "enabled", true);
    result.enabled = packageEnabled && targetEnabled;
    if (applyConfiguration && jsonContains(config, "mode"))
      result.mode = jsonString(config, "mode");
    const auto changes = jsonObject(config, "settings");
    if (applyConfiguration &&
        (!compositionMode(result.mode) ||
         !Settings::validatePluginSchema(result.settingsSchema, &result.error) ||
         !Settings::validateValues(result.settingsSchema, changes,
                                   &result.error))) {
      if (result.error.empty())
        result.error = "Invalid plugin target configuration";
      result.enabled = false;
      return {result};
    }
    for (auto it = changes.begin(); it != changes.end(); ++it)
      result.settings[it.key()] = it.value();

    results.push_back(result);
  }

  return results;
}

PluginDescriptor readPluginMetadata(const std::string &path,
                                    bool applyConfiguration) {
  const auto values = readPluginMetadataTargets(path, applyConfiguration);
  return values.empty() ? errorDescriptor(path, "Plugin has no targets")
                        : values.front();
}

Json pluginDescriptorJson(const PluginDescriptor &plugin) {
  Json tags = Json::array();
  std::set<std::string> seen;
  const auto appendTag = [&](const std::string &tag) {
    if (!tag.empty() && !seen.contains(tag)) {
      seen.insert(tag);
      tags.push_back(tag);
    }
  };
  for (const auto &tag : plugin.tags)
    appendTag(tag);
  appendTag(plugin.type);
  appendTag(plugin.target);

  return {
      {"id", plugin.id},
      {"packageId", plugin.id},
      {"instanceId", pluginInstanceId(plugin)},
      {"name", plugin.name},
      {"description", plugin.description},
      {"version", plugin.version},
      {"author", plugin.author},
      {"icon", plugin.icon},
      {"tags", tags},
      {"type", plugin.type},
      {"target", plugin.target},
      {"mode", plugin.mode},
      {"schemaVersion", plugin.schemaVersion},
      {"windowTemplate",
       jsonString(plugin.implementation, "windowTemplate", "tiling")},
      {"enabled", plugin.enabled},
      {"error", plugin.error},
      {"settingsSchema", plugin.settingsSchema},
      {"settings", plugin.settings},
      {"restartRequired", false}};
}

std::vector<PluginDescriptor> discoverPlugins() {
  std::vector<std::string> roots;
  std::vector<std::string> dataDirectories{std::string(g_get_user_data_dir())};
  for (const char *const *directory = g_get_system_data_dirs(); *directory;
       ++directory)
    dataDirectories.emplace_back(*directory);
  for (const auto &path : dataDirectories) {
    // SDK 2 packages have one canonical location. Do not discover the old
    // lunadash/shell/plugins or ludash/plugins trees: stale files left by old
    // releases otherwise shadow Store packages with the same id.
    roots.push_back(path + "/lunadash/plugins");
  }
  std::error_code executablePathError;
  const auto executable =
      std::filesystem::read_symlink("/proc/self/exe", executablePathError);
  if (!executablePathError)
    roots.push_back(executable.parent_path().string() + "/plugins");

  std::vector<PluginDescriptor> result;
  std::set<std::string> seenPackages;
  for (const auto &root : roots) {
    std::error_code error;
    std::vector<std::string> names;
    for (const auto &entry : std::filesystem::directory_iterator(root, error))
      if (entry.is_directory(error))
        names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    for (const auto &name : names) {
      const auto values = readPluginMetadataTargets(
          (std::filesystem::path(root) / name / "metadata.json").string());
      if (values.empty())
        continue;
      const auto id = values.front().id;
      if (!id.empty() && seenPackages.contains(id))
        continue;
      if (!id.empty())
        seenPackages.insert(id);
      result.insert(result.end(), values.begin(), values.end());
    }
  }
  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
    return a.id == b.id ? a.target < b.target : a.id < b.id;
  });
  return result;
}

} // namespace LunaDash
