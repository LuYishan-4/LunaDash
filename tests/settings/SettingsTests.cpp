#include "compositor/settings/SettingsApi.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "core/settings/SettingsTarget.hpp"
#include "shell/modules/ShellModules.hpp"
#include "shell/modules/ShellModuleSchema.hpp"
#include "shell/launcher/OrbitSettings.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/appearance/AppearancePresets.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include <QDir>
#include <QColor>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace LunaDash {
class SettingsTests final : public QObject {
  Q_OBJECT
  QTemporaryDir root;
  static QJsonObject request(const QJsonObject &target, const QJsonObject &changes) {
    return {{"target", target.value("id")}, {"revision", target.value("revision")},
            {"changes", changes}};
  }
  static QJsonObject descriptor(PluginManager &plugins, ShellModules &modules,
                                const QString &id) {
    for (const auto &entry : settingsApiTargets(plugins.snapshot(), modules.snapshot(), {}))
      if (entry.toObject().value("id").toString() == id) return entry.toObject();
    return {};
  }
private Q_SLOTS:
  void initTestCase() {
    QVERIFY(root.isValid());
    qputenv("XDG_CONFIG_HOME", (root.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (root.path() + "/data").toUtf8());
    qputenv("XDG_DATA_DIRS", (root.path() + "/empty").toUtf8());
    qputenv("LUNADASH_PLUGIN_CATALOG_URL", "off");
    QCoreApplication::setOrganizationName("LunaDashTests");
    QCoreApplication::setApplicationName("SettingsApi");
  }
  void legacyStoredNumbersKeepTheirTypes() {
    const QJsonObject rule{{"type", "integer"}, {"default", 12},
                            {"minimum", 0}, {"maximum", 64}};
    const auto restored = Settings::fromStoredValue(rule, QString("24"));
    QVERIFY(restored.isDouble());
    QCOMPARE(restored.toInt(), 24);
    QVERIFY(Settings::validValue(rule, restored));
    // The same string is not accepted as a typed API request.
    QVERIFY(!Settings::validValue(rule, QJsonValue("24")));
    QVERIFY(!Settings::validValue(rule, Settings::fromStoredValue(rule, QString("nan"))));
  }
  void sharedContractFixtures() {
    QFile file(QStringLiteral(SETTINGS_FIXTURES));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto cases = QJsonDocument::fromJson(file.readAll()).array();
    QVERIFY(!cases.isEmpty());
    for (const auto &entry : cases) {
      const auto test = entry.toObject();
      const auto rule = test.value("rule").toObject();
      QString error;
      const bool valid = Settings::validateSchema({{"option", rule}}, &error);
      QCOMPARE(valid, test.value("valid").toBool(true));
      if (!valid) continue;
      QCOMPARE(Settings::control(rule), test.value("control").toString());
      for (const auto &value : test.value("good").toArray())
        QVERIFY(Settings::validValue(rule, value));
      for (const auto &value : test.value("bad").toArray())
        QVERIFY(!Settings::validValue(rule, value));
    }
  }
  void actionsRejectMalformedPayloads() {
    const auto &strategy = windowTemplateForKey("tiling");
    auto layout = createWindowLayout(strategy);
    QVERIFY(layout->insert(0, 1));
    QVERIFY(layout->insert(0, 2));
    layout->layout(0, QRect(0, 0, 1200, 800));
    const auto before = layout->snapshot(0).focusedWindow;
    QVERIFY(!performWindowLayoutAction(*layout, strategy, "focus-direction", {{"dx", 1}}));
    QVERIFY(!performWindowLayoutAction(*layout, strategy, "focus-direction",
        {{"workspace", 0}, {"dx", 1}, {"dy", 1}}));
    QVERIFY(!performWindowLayoutAction(*layout, strategy, "resize-width",
        {{"window", 1}, {"width", -1}}));
    QVERIFY(!performWindowLayoutAction(*layout, strategy, "resize-width",
        {{"window", 1}, {"width", "900"}}));
    QVERIFY(!performWindowLayoutAction(*layout, strategy, "group-direction",
        {{"workspace", 1}, {"window", 1}, {"dx", 1}, {"dy", 0}}));
    QCOMPARE(layout->snapshot(0).focusedWindow, before);
  }
  void patchRevisionsAndReadOnly() {
    const QJsonObject schema{{"locked", QJsonObject{{"type", "boolean"},
        {"default", true}, {"readOnly", true}}}};
    const auto target = Settings::target("test", "Test", "effect", "Windows",
                                        schema, {{"locked", true}});
    QString error;
    QVERIFY(Settings::checkPatch(target, request(target, {{"locked", true}}), &error));
    QVERIFY(!Settings::checkPatch(target, request(target, {{"locked", false}}), &error));
    QVERIFY(!Settings::checkPatch(target, request(target, {{"unknown", 1}}), &error));
    auto stale = request(target, {});
    stale["revision"] = "old";
    QVERIFY(!Settings::checkPatch(target, stale, &error));
  }
  void modulesKeepHostInvariants() {
    QJsonObject normalized;
    QString error;
    const auto defaults = defaultModuleDocument();
    QVERIFY(validateModuleDocument(QJsonDocument(defaults).toJson(), &normalized, &error));
    QCOMPARE(normalized, defaults);
    QCOMPARE(shellModuleIds().size(), 11);
    QVERIFY(shellModuleIds().contains("orbit"));
    QVERIFY(shellModuleIds().contains("dock"));
    QVERIFY(!validateModuleDocument(R"({"schemaVersion":1,"modules":{"settings":{"enabled":false}}})", &normalized, &error));
    QVERIFY(!validateModuleDocument(R"({"schemaVersion":1,"modules":{"panel":{"config":{"workspaceInactiveWidth":48,"workspaceActiveWidth":18}}}})", &normalized, &error));
    QVERIFY(!validateModuleDocument(R"({"schemaVersion":1,"modules":{"panel":{"custom":{"entry":"../Main.qml"}}}})", &normalized, &error));
    QVERIFY(!validateModuleDocument(R"({"schemaVersion":1,"modules":{"panel":{"custom":{"enabled":true,"entry":""}}}})", &normalized, &error));
    QVERIFY(!validateModuleDocument(QByteArray(16385, ' '), &normalized, &error));
  }
  void orbitRejectsInvalidDocumentsWithoutLosingConfiguration() {
    QString error;
    QVERIFY(resetOrbitSettings(&error));
    const auto original = orbitSettings().value("document").toObject();
    QVERIFY(!original.isEmpty());
    auto invalid = original;
    invalid["defaultEngine"] = "missing";
    QVERIFY(!saveOrbitSettings(QJsonDocument(invalid).toJson(), &error));
    invalid = original;
    invalid["items"] = QJsonArray{QJsonObject{{"id", "bad"}, {"name", "Bad"},
        {"command", "sh -c echo test"}}};
    QVERIFY(!saveOrbitSettings(QJsonDocument(invalid).toJson(), &error));
    invalid["items"] = QJsonArray{QJsonObject{{"id", "bad"}, {"name", "Bad"},
        {"url", "file:///etc/passwd"}}};
    QVERIFY(!saveOrbitSettings(QJsonDocument(invalid).toJson(), &error));
    QVERIFY(!saveOrbitSettings(QByteArray(32769, ' '), &error));
    QCOMPARE(orbitSettings().value("document").toObject(), original);
  }
  void appearancePresetsValidateBeforeApplying() {
    QString error;
    QVERIFY(updateDesktopPreferences({{"themeMode", "light"}, {"eyeCareTemperature", 4200}}, &error));
    QVERIFY(saveAppearancePreset("daylight", &error));
    QVERIFY(!saveAppearancePreset("../outside", &error));
    QVERIFY(updateDesktopPreferences({{"themeMode", "dark"}}, &error));
    QVERIFY(applyAppearancePreset("daylight", &error));
    QCOMPARE(desktopPreferences().value("themeMode").toString(), QString("light"));
    const auto before = desktopPreferences();
    QVERIFY(!updateDesktopPreferences({{"themeMode", "dark"}, {"eyeCareTemperature", 0}}, &error));
    QCOMPARE(desktopPreferences(), before);
    QVERIFY(!updateDesktopPreferences({{"weatherLatitude", 91}}, &error));
    QVERIFY(!updateDesktopPreferences({{"wallpaperDirectory", "relative"}}, &error));
    QVERIFY(deleteAppearancePreset("daylight", &error));
  }
  void paletteFollowsDesktopColorScheme() {
    const auto light = appearancePalette({{"themeMode", "light"}, {"accent", "#6699cc"}});
    const auto dark = appearancePalette({{"themeMode", "dark"}, {"accent", "#6699cc"}});
    QVERIFY(!light.value("dark").toBool());
    QVERIFY(dark.value("dark").toBool());
    QVERIFY(QColor(light.value("text").toString()).lightnessF() <
            QColor(light.value("background").toString()).lightnessF());
    QVERIFY(QColor(dark.value("text").toString()).lightnessF() >
            QColor(dark.value("background").toString()).lightnessF());
  }
  void moduleApiPersistsAndRejectsStaleEdits() {
    PluginManager plugins;
    ShellModules modules;
    const auto noLayout = [](const QJsonObject &, QString *) { return false; };
    auto target = descriptor(plugins, modules, "module:panel:config");
    QVERIFY(!target.isEmpty());
    QString error;
    const auto edit = request(target, {{"shellOpacity", 35}});
    QVERIFY2(updateSettingsApi(plugins, modules, {}, edit, noLayout, &error), qPrintable(error));
    QCOMPARE(descriptor(plugins, modules, "module:panel:config").value("values").toObject().value("shellOpacity").toInt(), 35);
    QVERIFY(!updateSettingsApi(plugins, modules, {}, edit, noLayout, &error));
    target = descriptor(plugins, modules, "module:panel:config");
    const auto before = modules.snapshot().value("document");
    QVERIFY(!updateSettingsApi(plugins, modules, {}, request(target, {{"shellOpacity", 99}}), noLayout, &error));
    QCOMPARE(modules.snapshot().value("document"), before);
    target = descriptor(plugins, modules, "module:settings:module");
    QVERIFY(!updateSettingsApi(plugins, modules, {}, request(target, {{"enabled", false}}), noLayout, &error));
  }
  void effectSettingsReachNativeHook() {
    PluginManager plugins;
    plugins.refresh();
    ShellModules modules;
    auto target = descriptor(plugins, modules, "plugin:org.ludash.fade");
    if (target.isEmpty()) QSKIP("Example effects disabled");
    QString error;
    const auto noLayout = [](const QJsonObject &, QString *) { return false; };
    QVERIFY2(updateSettingsApi(plugins, modules, {}, request(target,
        {{"duration", 300}, {"softFocus", false}, {"easing", "linear"}, {"exitScale", 0.75}}),
        noLayout, &error), qPrintable(error));
    auto document = readExtensionConfiguration();
    auto entries = document.value("plugins").toObject();
    auto effect = entries.value("org.ludash.fade").toObject();
    effect["enabled"] = true;
    entries["org.ludash.fade"] = effect;
    document["plugins"] = entries;
    QVERIFY2(saveExtensionConfiguration(QJsonDocument(document).toJson(), &error), qPrintable(error));
    plugins.refresh();
    const QJsonObject preferences{{"animations", true}, {"animationDuration", 220}};
    const auto profile = windowAnimationProfile(plugins, preferences, windowTemplateForKey("tiling"));
    QCOMPARE(profile.value("duration").toInt(), 300);
    QCOMPARE(profile.value("focusOpacity").toDouble(), 1.0);
    QCOMPARE(profile.value("exitScale").toDouble(), 0.75);
    QCOMPARE(profile.value("easing").toString(), QString("linear"));
  }
};
}
QTEST_GUILESS_MAIN(LunaDash::SettingsTests)
#include "SettingsTests.moc"
