#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "config/plugins/PluginCatalog.hpp"
#include "core/crypto/Sha256.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/filesystem/TemporaryDirectory.hpp"
#include "core/settings/SettingsSchema.hpp"
#include "support/TestHarness.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

namespace LunaDash {
namespace {
bool writeText(const std::filesystem::path &path, const std::string &bytes) {
  return writeFileAtomically(path, bytes);
}

bool removeTree(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove_all(path, error);
  return !error && !std::filesystem::exists(path);
}

// A native effect package. The library is a placeholder: enabling it records a
// load error, which the tests below do not depend on.
bool createEffectPlugin(const std::filesystem::path &base, const std::string &id,
                        const std::string &target,
                        const std::string &version = "1.0.0") {
  const auto directory = base / id;
  if (!createDirectories(directory))
    return false;
  const Json manifest{{"schemaVersion", 2},
                      {"sdk", Json{{"name", "LunaDash"}, {"apiVersion", 2}}},
                      {"id", id},
                      {"name", id},
                      {"version", version},
                      {"author", "Plugin tests"},
                      {"type", "effect"},
                      {"target", target},
                      {"mode", "augment"},
                      {"entry", "effect.so"},
                      {"enabledByDefault", true},
                      {"settings", Json::object()}};
  const auto metadata = jsonCompact(manifest);
  if (!writeText(directory / "metadata.json", metadata) ||
      !writeText(directory / "effect.so", "placeholder\n"))
    return false;
  return writeText(directory / ".lunadash-sdk.json",
                   jsonCompact(Json{{"apiVersion", 2},
                                    {"metadataSha256", sha256Hex(metadata)}}));
}

Json findById(const Json &items, const std::string &id) {
  for (const auto &item : items)
    if (jsonString(item, "id") == id)
      return item;
  return Json();
}
} // namespace

class PluginTests final {
  TemporaryDirectory temporary;
  std::filesystem::path root;

  static Json config(const std::string &id, bool enabled,
                     const std::string &mode, Json settings = Json::object()) {
    return {{"schemaVersion", 1},
            {"builtins", Json::object()},
            {"plugins", Json{{id, Json{{"enabled", enabled},
                                       {"mode", mode},
                                       {"settings", settings}}}}}};
  }
  static void save(const Json &object) {
    std::string error;
    VERIFY2(saveExtensionConfiguration(jsonCompact(object), &error), error);
  }

public:
  void initTestCase() {
    VERIFY(temporary.isValid());
    const auto base = temporary.path().string();
    ::setenv("XDG_CONFIG_HOME", (base + "/config").c_str(), 1);
    ::setenv("XDG_DATA_HOME", (base + "/data").c_str(), 1);
    ::setenv("XDG_DATA_DIRS", (base + "/empty").c_str(), 1);
    ::setenv("LUNADASH_PLUGIN_CATALOG_URL", "off", 1);
    root = temporary.path() / "data/lunadash/plugins";
  }

  void sameVersionStoreRevision() {
    TemporaryDirectory package;
    VERIFY(package.isValid());
    PluginDescriptor plugin;
    plugin.id = "org.example.spectrum";
    plugin.version = "1.0.2a";
    plugin.metadataPath = package.filePath("metadata.json");
    Json file{{"path", "Effect.cpp"},
              {"sha256", "first"},
              {"url", "https://example.test/a"}};
    Json item{{"id", plugin.id},
              {"version", plugin.version},
              {"install", Json{{"files", Json::array({file})}}}};
    VERIFY(!pluginMatchesStore(plugin, item));
    VERIFY(writeText(package.filePath(".lunadash-store.json"),
                     jsonCompact(Json{
                         {"fingerprint", pluginStoreFingerprint(item)}})));
    VERIFY(pluginMatchesStore(plugin, item));
    file["url"] = "https://example.test/moved";
    item["install"] = Json{{"files", Json::array({file})}};
    VERIFY(pluginMatchesStore(plugin, item));
    file["sha256"] = "second";
    item["install"] = Json{{"files", Json::array({file})}};
    VERIFY(!pluginMatchesStore(plugin, item));
  }

  void pluginSettingsUseStableControls() {
    std::string error;
    VERIFY(Settings::validatePluginSchema(
        Json{{"enabled", Json{{"type", "boolean"},
                              {"default", false},
                              {"control", "toggle"}}},
             {"mode", Json{{"type", "string"},
                           {"default", "a"},
                           {"enum", Json::array({"a", "b"})},
                           {"control", "select"}}},
             {"amount", Json{{"type", "integer"},
                             {"default", 2},
                             {"control", "number"}}},
             {"strength", Json{{"type", "number"},
                               {"default", 0.5},
                               {"minimum", 0.0},
                               {"maximum", 1.0},
                               {"control", "slider"}}}},
        &error));
    VERIFY(!Settings::validatePluginSchema(
        Json{{"text", Json{{"type", "string"},
                           {"default", "custom"},
                           {"control", "text"}}}},
        &error));
    VERIFY(!Settings::validatePluginSchema(
        Json{{"many",
              Json{{"type", "array"},
                   {"default", Json::array()},
                   {"items", Json{{"type", "string"},
                                  {"enum", Json::array({"a"})}}},
                   {"control", "select"}}}},
        &error));
  }

  void registryAndValidation() {
    COMPARE(extensionTargets().size(), std::size_t{4});
    std::set<std::string> ids;
    for (const auto &target : extensionTargets()) {
      const auto id = jsonString(target, "id");
      VERIFY(!ids.contains(id));
      ids.insert(id);
      const auto settings = jsonObject(target, "settings");
      VERIFY(validateExtensionSettings(settings, extensionDefaults(settings),
                                       nullptr));
    }
    std::string error;
    VERIFY(!saveExtensionConfiguration(
        R"({"schemaVersion":1,"builtins":{"missing":{}},"plugins":{}})",
        &error));
    VERIFY(!saveExtensionConfiguration(
        R"({"schemaVersion":1,"builtins":{"window-animation":{"duration":9999}},"plugins":{}})",
        &error));
    VERIFY(!saveExtensionConfiguration(
        R"({"schemaVersion":1,"builtins":{},"plugins":{"unknown":{"enabled":true}}})",
        &error));
  }

  void bundledStoreRegistry() {
    PluginManager manager;
    const auto snapshot = manager.snapshot();
    const auto remote = jsonArray(snapshot, "remote");
    COMPARE(remote.size(), std::size_t{1});

    const auto kde = findById(remote, "org.lunadash.kde-behavior");
    VERIFY(kde.is_object());
    COMPARE(jsonArray(kde, "targets").size(), std::size_t{2});
    VERIFY(jsonBool(kde, "installable"));
    COMPARE(jsonString(jsonObject(kde, "install"), "mode"), std::string("cmake"));
    VERIFY(jsonArray(jsonObject(kde, "install"), "files").size() >= 3);

    const auto source = jsonString(kde, "sourceUrl");
    VERIFY2(source.starts_with("https://github.com/LuYishan-4/LunaDash-Plugins/"),
            source);
    VERIFY(jsonBool(snapshot, "storeSupported"));
    VERIFY(!jsonBool(snapshot, "storeLoading"));
  }

  void hidesLegacyPluginsAndRemovesUserPackages() {
    const auto legacyDir = root / "org.example.legacy";
    VERIFY(createDirectories(legacyDir));
    VERIFY(writeText(
        legacyDir / "metadata.json",
        R"({"schemaVersion":1,"id":"org.example.legacy","name":"Legacy","version":"1","type":"quickshell","target":"desktop-widgets","entry":"Main.qml","enabledByDefault":false})"));
    VERIFY(writeText(legacyDir / "Main.qml", "import QtQuick\nItem {}\n"));

    VERIFY(createEffectPlugin(root, "org.example.removable", "window-rules"));

    PluginManager manager;
    manager.refresh();
    auto installed = jsonArray(manager.snapshot(), "installed");
    VERIFY(findById(installed, "org.example.legacy").is_null());
    const auto removable = findById(installed, "org.example.removable");
    VERIFY(removable.is_object() && jsonBool(removable, "removable"));

    std::string error;
    VERIFY2(manager.removeInstalledPlugin("org.example.removable", &error),
            error);
    VERIFY(!std::filesystem::exists(root / "org.example.removable"));
    installed = jsonArray(manager.snapshot(), "installed");
    VERIFY(findById(installed, "org.example.removable").is_null());
    VERIFY(removeTree(legacyDir));
  }

  void visualPluginTypesAreRejected() {
    const auto directory = root / "org.example.widget";
    VERIFY(createDirectories(directory));
    const std::string metadata =
        R"({"schemaVersion":2,"sdk":{"name":"LunaDash","apiVersion":2},"id":"org.example.widget","name":"Widget","version":"1.0.0","author":"test","type":"quickshell","target":"window-rules","mode":"augment","entry":"Main.qml","enabledByDefault":false,"settings":{}})";
    VERIFY(writeText(directory / "metadata.json", metadata));
    VERIFY(writeText(directory / "Main.qml", "import QtQuick\nItem {}\n"));
    VERIFY(writeText(directory / ".lunadash-sdk.json",
                     jsonCompact(Json{{"apiVersion", 2},
                                      {"metadataSha256", sha256Hex(metadata)}})));

    PluginManager manager;
    manager.refresh();
    const auto widget =
        findById(jsonArray(manager.snapshot(), "installed"), "org.example.widget");
    VERIFY(widget.is_object());
    VERIFY(!jsonString(widget, "error").empty());
    VERIFY(!jsonBool(widget, "available"));
    VERIFY(removeTree(directory));
  }

  void outdatedStoreVersionRemainsOperable() {
    const auto directory = root / "org.lunadash.kde-behavior";
    VERIFY(createEffectPlugin(root, "org.lunadash.kde-behavior", "window-rules",
                              "0.9.0"));

    PluginManager manager;
    manager.refresh();
    const auto snapshot = manager.snapshot();
    const auto old = findById(jsonArray(snapshot, "installed"),
                              "org.lunadash.kde-behavior");
    VERIFY(old.is_object());
    VERIFY(jsonBool(old, "outdated"));
    COMPARE(jsonString(old, "storeVersion"), std::string("1.0.0"));
    VERIFY(jsonBool(old, "removable"));

    const auto store =
        findById(jsonArray(snapshot, "remote"), "org.lunadash.kde-behavior");
    VERIFY(store.is_object());
    VERIFY(jsonBool(store, "updateAvailable"));

    VERIFY(removeTree(directory));
  }

  void legacyDestinationCanBeUpdatedWhenIdentityMatches() {
    const auto directory = root / "org.lunadash.kde-behavior";
    VERIFY(createDirectories(directory));
    VERIFY(writeText(
        directory / "manifest.json",
        R"({"id":"org.lunadash.kde-behavior","name":"KDE Behavior","version":"0.1.0"})"));

    PluginManager manager;
    std::string error;
    // The one-click update is asynchronous; success here means the stale
    // destination was accepted and the download/build pipeline was started
    // instead of returning "Plugin destination already exists".
    VERIFY2(manager.installFromStore("org.lunadash.kde-behavior", &error),
            error);
    VERIFY(error.empty());
    VERIFY(removeTree(directory));
  }

  void configurationRecovery() {
    std::string error;
    VERIFY(!saveExtensionConfiguration(
        R"({"schemaVersion":1,"builtins":{},"plugins":{"absent":{"enabled":false,"settings":{},"mode":"replace","unexpected":1}}})",
        &error));
    auto document = config("absent", false, "replace");
    save(document);
    COMPARE(readExtensionConfiguration(), document);
    const auto previous = readExtensionConfiguration();
    VERIFY(!saveExtensionConfiguration(std::string(24577, ' '), &error));
    COMPARE(readExtensionConfiguration(), previous);
  }

  void exclusiveTargetOwnership() {
    const auto first = root / "org.example.rules-a";
    const auto second = root / "org.example.rules-b";
    VERIFY(createEffectPlugin(root, "org.example.rules-a", "window-rules"));
    VERIFY(createEffectPlugin(root, "org.example.rules-b", "window-rules"));

    const Json conflicting{
        {"schemaVersion", 1},
        {"builtins", Json::object()},
        {"plugins",
         Json{{"org.example.rules-a", Json{{"enabled", true},
                                           {"mode", "augment"},
                                           {"settings", Json::object()}}},
              {"org.example.rules-b", Json{{"enabled", true},
                                           {"mode", "augment"},
                                           {"settings", Json::object()}}}}}};
    std::string error;
    VERIFY(!saveExtensionConfiguration(jsonCompact(conflicting), &error));
    VERIFY2(error.find("Only one plugin can be enabled for: window-rules") !=
                std::string::npos,
            error);

    PluginManager manager;
    manager.refresh();
    // Both packages are enabled by default; the second one loses the target.
    // (The first one only fails to load its placeholder library.)
    int rulesPlugins = 0;
    int conflicts = 0;
    for (const auto &item : jsonArray(manager.snapshot(), "installed")) {
      if (jsonString(item, "target") != "window-rules" ||
          !jsonString(item, "id").starts_with("org.example.rules-"))
        continue;
      ++rulesPlugins;
      if (jsonString(item, "error").find("single-owner target") !=
          std::string::npos)
        ++conflicts;
    }
    COMPARE(rulesPlugins, 2);
    COMPARE(conflicts, 1);

    VERIFY(removeTree(first));
    VERIFY(removeTree(second));
  }
};
} // namespace LunaDash

int main(int argc, char **argv) {
  using Fixture = LunaDash::PluginTests;
  return LunaDash::Test::run<Fixture>(
      "PluginTests",
      {{"initTestCase", &Fixture::initTestCase},
       {"sameVersionStoreRevision", &Fixture::sameVersionStoreRevision},
       {"pluginSettingsUseStableControls",
        &Fixture::pluginSettingsUseStableControls},
       {"registryAndValidation", &Fixture::registryAndValidation},
       {"bundledStoreRegistry", &Fixture::bundledStoreRegistry},
       {"hidesLegacyPluginsAndRemovesUserPackages",
        &Fixture::hidesLegacyPluginsAndRemovesUserPackages},
       {"visualPluginTypesAreRejected", &Fixture::visualPluginTypesAreRejected},
       {"outdatedStoreVersionRemainsOperable",
        &Fixture::outdatedStoreVersionRemainsOperable},
       {"legacyDestinationCanBeUpdatedWhenIdentityMatches",
        &Fixture::legacyDestinationCanBeUpdatedWhenIdentityMatches},
       {"configurationRecovery", &Fixture::configurationRecovery},
       {"exclusiveTargetOwnership", &Fixture::exclusiveTargetOwnership}},
      argc, argv);
}
