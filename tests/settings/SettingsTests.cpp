#include "compositor/settings/SettingsApi.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include "config/appearance/AppearancePresets.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/filesystem/TemporaryDirectory.hpp"
#include "core/settings/SettingsTarget.hpp"
#include "shell/launcher/OrbitSettings.hpp"
#include "shell/modules/ShellModuleSchema.hpp"
#include "shell/modules/ShellModules.hpp"
#include "support/TestHarness.hpp"

#include <glib.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {
double hslLightness(const std::string &text) {
  std::array<int, 3> channels{};
  for (std::size_t index = 0; index < channels.size(); ++index)
    channels[index] = std::stoi(text.substr(1 + index * 2, 2), nullptr, 16);
  const auto [minimum, maximum] =
      std::minmax_element(channels.begin(), channels.end());
  return (*minimum + *maximum) / 510.0;
}
} // namespace

namespace LunaDash {

class SettingsTests final {
  TemporaryDirectory root;
  static Json request(const Json &target, const Json &changes) {
    return {{"target", jsonValue(target, "id")},
            {"revision", jsonValue(target, "revision")},
            {"changes", changes}};
  }
  static Json descriptor(PluginManager &plugins, ShellModules &modules,
                         const std::string &id) {
    for (const auto &entry : settingsApiTargets(plugins.snapshot(),
                                                modules.snapshot(),
                                                Json::object()))
      if (jsonString(entry, "id") == id)
        return entry;
    return Json::object();
  }
  std::string rootPath() const { return root.path().string(); }

public:
  void initTestCase() {
    VERIFY(root.isValid());
    ::setenv("XDG_CONFIG_HOME", (rootPath() + "/config").c_str(), 1);
    ::setenv("XDG_DATA_HOME", (rootPath() + "/data").c_str(), 1);
    ::setenv("XDG_DATA_DIRS", (rootPath() + "/empty").c_str(), 1);
    ::setenv("LUNADASH_PLUGIN_CATALOG_URL", "off", 1);
  }

  void sharedContractFixtures() {
    const auto contents = readFile(SETTINGS_FIXTURES);
    VERIFY(contents.has_value());
    const auto cases = parseJson(*contents);
    VERIFY(cases.is_array() && !cases.empty());
    for (const auto &test : cases) {
      const auto rule = jsonObject(test, "rule");
      std::string error;
      const bool valid = Settings::validateSchema(Json{{"option", rule}}, &error);
      COMPARE(valid, jsonBool(test, "valid", true));
      if (!valid)
        continue;
      COMPARE(Settings::control(rule), jsonString(test, "control"));
      for (const auto &value : jsonArray(test, "good"))
        VERIFY(Settings::validValue(rule, value));
      for (const auto &value : jsonArray(test, "bad"))
        VERIFY(!Settings::validValue(rule, value));
    }
  }

  void actionsRejectMalformedPayloads() {
    const auto &strategy = windowTemplateForKey("tiling");
    auto layout = createWindowLayout(strategy);
    VERIFY(layout->insert(0, 1));
    VERIFY(layout->insert(0, 2));
    layout->layout(0, Rect(0, 0, 1200, 800));
    const auto before = layout->snapshot(0).focusedWindow;
    VERIFY(!performWindowLayoutAction(*layout, strategy, "focus-direction",
                                      {{"dx", 1}}));
    VERIFY(!performWindowLayoutAction(*layout, strategy, "focus-direction",
                                      {{"workspace", 0}, {"dx", 1}, {"dy", 1}}));
    VERIFY(!performWindowLayoutAction(*layout, strategy, "resize-width",
                                      {{"window", 1}, {"width", -1}}));
    VERIFY(!performWindowLayoutAction(*layout, strategy, "resize-width",
                                      {{"window", 1}, {"width", "900"}}));
    VERIFY(!performWindowLayoutAction(
        *layout, strategy, "group-direction",
        {{"workspace", 1}, {"window", 1}, {"dx", 1}, {"dy", 0}}));
    COMPARE(layout->snapshot(0).focusedWindow, before);
  }

  void patchRevisionsAndReadOnly() {
    const Json schema{{"locked", Json{{"type", "boolean"},
                                      {"default", true},
                                      {"readOnly", true}}}};
    const auto target = Settings::target("test", "Test", "effect", "Windows",
                                         schema, Json{{"locked", true}});
    std::string error;
    VERIFY(Settings::checkPatch(target, request(target, {{"locked", true}}),
                                &error));
    VERIFY(!Settings::checkPatch(target, request(target, {{"locked", false}}),
                                 &error));
    VERIFY(!Settings::checkPatch(target, request(target, {{"unknown", 1}}),
                                 &error));
    auto stale = request(target, Json::object());
    stale["revision"] = "old";
    VERIFY(!Settings::checkPatch(target, stale, &error));
  }

  void modulesKeepHostInvariants() {
    ShellModuleJson normalized;
    std::string error;
    const auto defaults = defaultModuleDocument();
    VERIFY(validateModuleDocument(defaults.dump(), &normalized, &error));
    VERIFY(normalized == defaults);
    VERIFY(shellModuleIds().size() == 11);
    const auto ids = shellModuleIds();
    VERIFY(std::find(ids.begin(), ids.end(), "orbit") != ids.end());
    VERIFY(std::find(ids.begin(), ids.end(), "dock") != ids.end());
    VERIFY(!validateModuleDocument(
        R"({"schemaVersion":1,"modules":{"settings":{"enabled":false}}})",
        &normalized, &error));
    VERIFY(!validateModuleDocument(
        R"({"schemaVersion":1,"modules":{"panel":{"config":{"workspaceInactiveWidth":48,"workspaceActiveWidth":18}}}})",
        &normalized, &error));
    // Legacy code fields are ignored without losing safe style/config values.
    VERIFY(validateModuleDocument(
        R"({"schemaVersion":1,"modules":{"panel":{"style":{"height":48},"custom":{"enabled":true,"entry":"../Main.qml"}}}})",
        &normalized, &error));
    const auto panel = normalized.at("modules").at("panel");
    VERIFY(!panel.contains("custom"));
    COMPARE(panel.at("style").at("height").get<int>(), 48);
    VERIFY(!shellModuleDescriptor("panel").at("sections").contains("custom"));
    VERIFY(!validateModuleDocument(std::string(16385, ' '), &normalized, &error));
  }

  void moduleMonitorReloadsExternalChanges() {
    ShellModules modules;
    int changes = 0;
    modules.setChangedCallback([&changes] { ++changes; });
    auto document = defaultModuleDocument();
    document["modules"]["panel"]["config"]["shellOpacity"] = 41;
    const auto path = modules.snapshot().at("path").get<std::string>();
    {
      std::ofstream file(path, std::ios::binary | std::ios::trunc);
      VERIFY(file.good());
      file << document.dump();
      VERIFY(file.good());
    }
    // GIO file monitors dispatch through the default GLib main context.
    VERIFY(LunaDash::Test::waitUntil(
        [&] { return changes > 0; }, 2000,
        [] { g_main_context_iteration(nullptr, FALSE); }));
    COMPARE(modules.snapshot()
                .at("document")
                .at("modules")
                .at("panel")
                .at("config")
                .at("shellOpacity")
                .get<int>(),
            41);
  }

  void orbitRejectsInvalidDocumentsWithoutLosingConfiguration() {
    std::string error;
    VERIFY(resetOrbitSettings(&error));
    const auto original = orbitSettings().at("document");
    VERIFY(!original.empty());
    auto invalid = original;
    invalid["defaultEngine"] = "missing";
    VERIFY(!saveOrbitSettings(invalid.dump(), &error));
    invalid = original;
    invalid["items"] = nlohmann::json::array(
        {{{"id", "bad"}, {"name", "Bad"}, {"command", "sh -c echo test"}}});
    VERIFY(!saveOrbitSettings(invalid.dump(), &error));
    invalid["items"] = nlohmann::json::array(
        {{{"id", "bad"}, {"name", "Bad"}, {"url", "file:///etc/passwd"}}});
    VERIFY(!saveOrbitSettings(invalid.dump(), &error));
    VERIFY(!saveOrbitSettings(std::string(32769, ' '), &error));
    VERIFY(orbitSettings().at("document") == original);
  }

  void appearancePresetsValidateBeforeApplying() {
    std::string error;
    std::string presetError;
    VERIFY(updateDesktopPreferences(
        {{"themeMode", "light"}, {"eyeCareTemperature", 4200}}, &error));
    VERIFY(saveAppearancePreset("daylight", &presetError));
    VERIFY(!saveAppearancePreset("../outside", &presetError));
    VERIFY(updateDesktopPreferences({{"themeMode", "dark"}}, &error));
    VERIFY(applyAppearancePreset("daylight", &presetError));
    VERIFY(desktopPreferences().at("themeMode") == "light");
    const auto before = desktopPreferences();
    VERIFY(!updateDesktopPreferences(
        {{"themeMode", "dark"}, {"eyeCareTemperature", 0}}, &error));
    VERIFY(desktopPreferences() == before);
    VERIFY(!updateDesktopPreferences({{"weatherLatitude", 91}}, &error));
    VERIFY(!updateDesktopPreferences({{"wallpaperDirectory", "relative"}},
                                     &error));
    VERIFY(deleteAppearancePreset("daylight", &presetError));
  }

  void paletteFollowsDesktopColorScheme() {
    const auto light = appearancePalette(
        nlohmann::json{{"themeMode", "light"}, {"accent", "#6699cc"}});
    const auto dark = appearancePalette(
        nlohmann::json{{"themeMode", "dark"}, {"accent", "#6699cc"}});
    VERIFY(!light.value("dark", true));
    VERIFY(dark.value("dark", false));
    VERIFY(hslLightness(light.at("text").get<std::string>()) <
           hslLightness(light.at("background").get<std::string>()));
    VERIFY(hslLightness(dark.at("text").get<std::string>()) >
           hslLightness(dark.at("background").get<std::string>()));
  }

  void moduleApiPersistsAndRejectsStaleEdits() {
    PluginManager plugins;
    ShellModules modules;
    const auto noLayout = [](const Json &, std::string *) { return false; };
    auto target = descriptor(plugins, modules, "module:panel:config");
    VERIFY(!target.empty());
    std::string error;
    const auto edit = request(target, {{"shellOpacity", 35}});
    VERIFY2(updateSettingsApi(plugins, modules, Json::object(), edit, noLayout,
                              &error),
            error);
    COMPARE(jsonInt(jsonObject(descriptor(plugins, modules,
                                          "module:panel:config"),
                               "values"),
                    "shellOpacity"),
            35);
    VERIFY(!updateSettingsApi(plugins, modules, Json::object(), edit, noLayout,
                              &error));
    target = descriptor(plugins, modules, "module:panel:config");
    const auto before = modules.snapshot().at("document");
    VERIFY(!updateSettingsApi(plugins, modules, Json::object(),
                              request(target, {{"shellOpacity", 99}}), noLayout,
                              &error));
    COMPARE(modules.snapshot().at("document").dump(), before.dump());
    target = descriptor(plugins, modules, "module:settings:module");
    VERIFY(!updateSettingsApi(plugins, modules, Json::object(),
                              request(target, {{"enabled", false}}), noLayout,
                              &error));
  }
};
} // namespace LunaDash

int main(int argc, char **argv) {
  using Fixture = LunaDash::SettingsTests;
  return LunaDash::Test::run<Fixture>(
      "SettingsTests",
      {{"initTestCase", &Fixture::initTestCase},
       {"sharedContractFixtures", &Fixture::sharedContractFixtures},
       {"actionsRejectMalformedPayloads",
        &Fixture::actionsRejectMalformedPayloads},
       {"patchRevisionsAndReadOnly", &Fixture::patchRevisionsAndReadOnly},
       {"modulesKeepHostInvariants", &Fixture::modulesKeepHostInvariants},
       {"moduleMonitorReloadsExternalChanges",
        &Fixture::moduleMonitorReloadsExternalChanges},
       {"orbitRejectsInvalidDocumentsWithoutLosingConfiguration",
        &Fixture::orbitRejectsInvalidDocumentsWithoutLosingConfiguration},
       {"appearancePresetsValidateBeforeApplying",
        &Fixture::appearancePresetsValidateBeforeApplying},
       {"paletteFollowsDesktopColorScheme",
        &Fixture::paletteFollowsDesktopColorScheme},
       {"moduleApiPersistsAndRejectsStaleEdits",
        &Fixture::moduleApiPersistsAndRejectsStaleEdits}},
      argc, argv);
}
