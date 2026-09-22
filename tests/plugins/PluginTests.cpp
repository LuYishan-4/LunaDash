#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "config/plugins/PluginCatalog.hpp"
#include "core/settings/SettingsSchema.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

namespace LunaDash {
class PluginTests : public QObject {
  Q_OBJECT
  QTemporaryDir temporary;
  QString root;
  static bool copyPlugin(const QString &from, const QString &to) {
    QDir().mkpath(to);
    for (const auto &name : QDir(from).entryList(QDir::Files | QDir::Hidden))
      if (!QFile::copy(from + "/" + name, to + "/" + name))
        return false;
    return QFile::exists(to + "/metadata.json");
  }
  static bool writeFile(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(bytes) == bytes.size();
  }
  static bool createQmlPlugin(const QString &base, const QString &id,
                              const QString &target) {
    const auto directory = base + "/" + id;
    if (!QDir().mkpath(directory))
      return false;
    const QJsonObject manifest{
        {"schemaVersion", 2},
        {"sdk", QJsonObject{{"name", "LunaDash"}, {"apiVersion", 2}}},
        {"id", id},
        {"name", id},
        {"version", "1.0.0"},
        {"author", "Plugin tests"},
        {"type", "quickshell"},
        {"target", target},
        {"mode", "augment"},
        {"entry", "Main.qml"},
        {"enabledByDefault", true},
        {"settings", QJsonObject{}}};
    const auto metadata =
        QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    if (!writeFile(directory + "/metadata.json", metadata) ||
        !writeFile(directory + "/Main.qml",
                   "import QtQuick\nItem { required property var shell; "
                   "required property var settings; required property var context }\n"))
      return false;
    const auto hash =
        QString::fromLatin1(QCryptographicHash::hash(
                                metadata, QCryptographicHash::Sha256)
                                .toHex());
    return writeFile(
        directory + "/.lunadash-sdk.json",
        QJsonDocument(QJsonObject{{"apiVersion", 2},
                                  {"metadataSha256", hash}})
                .toJson(QJsonDocument::Compact));
  }
  QJsonObject config(const QString &id, bool enabled, const QString &mode,
                     QJsonObject settings = {}) {
    return {
        {"schemaVersion", 1},
        {"builtins", QJsonObject{}},
        {"plugins", QJsonObject{{id, QJsonObject{{"enabled", enabled},
                                                 {"mode", mode},
                                                 {"settings", settings}}}}}};
  }
  void save(const QJsonObject &object) {
    QString error;
    QVERIFY2(saveExtensionConfiguration(QJsonDocument(object).toJson(), &error),
             qPrintable(error));
  }
private Q_SLOTS:
  void initTestCase() {
    QVERIFY(temporary.isValid());
    qputenv("XDG_CONFIG_HOME", (temporary.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (temporary.path() + "/data").toUtf8());
    qputenv("XDG_DATA_DIRS", (temporary.path() + "/empty").toUtf8());
    qputenv("LUNADASH_PLUGIN_CATALOG_URL", "off");
    QCoreApplication::setOrganizationName("LunaDashPluginTests");
    QCoreApplication::setApplicationName("Plugins");
    root = temporary.path() + "/data/lunadash/plugins";
  }
  void pluginSettingsUseStableControls() {
    QString error;
    QVERIFY(Settings::validatePluginSchema(
        {{"enabled", QJsonObject{{"type", "boolean"},
                                  {"default", false},
                                  {"control", "toggle"}}},
         {"mode", QJsonObject{{"type", "string"},
                               {"default", "a"},
                               {"enum", QJsonArray{"a", "b"}},
                               {"control", "select"}}},
         {"amount", QJsonObject{{"type", "integer"},
                                 {"default", 2},
                                 {"control", "number"}}},
         {"strength", QJsonObject{{"type", "number"},
                                   {"default", 0.5},
                                   {"minimum", 0.0},
                                   {"maximum", 1.0},
                                   {"control", "slider"}}}},
        &error));
    QVERIFY(!Settings::validatePluginSchema(
        {{"text", QJsonObject{{"type", "string"},
                               {"default", "custom"},
                               {"control", "text"}}}},
        &error));
    QVERIFY(!Settings::validatePluginSchema(
        {{"many", QJsonObject{{"type", "array"},
                               {"default", QJsonArray{}},
                               {"items", QJsonObject{{"type", "string"},
                                                     {"enum", QJsonArray{"a"}}}},
                               {"control", "select"}}}},
        &error));
  }

  void registryAndValidation() {
    QVERIFY(extensionTargets().size() > 35);
    QSet<QString> ids;
    for (const auto &target : extensionTargets()) {
      const auto object = target.toObject();
      QVERIFY(!ids.contains(object.value("id").toString()));
      ids.insert(object.value("id").toString());
      QVERIFY(validateExtensionSettings(
          object.value("settings").toObject(),
          extensionDefaults(object.value("settings").toObject()), nullptr));
    }
    QString error;
    QVERIFY(!saveExtensionConfiguration(
        R"({"schemaVersion":1,"builtins":{"missing":{}},"plugins":{}})",
        &error));
    QVERIFY(!saveExtensionConfiguration(
        R"({"schemaVersion":1,"builtins":{"window-animation":{"duration":9999}},"plugins":{}})",
        &error));
    QVERIFY(!saveExtensionConfiguration(
        R"({"schemaVersion":1,"builtins":{},"plugins":{"unknown":{"enabled":true}}})",
        &error));
  }
  void bundledStoreRegistry() {
    PluginManager manager;
    const auto snapshot = manager.snapshot();
    const auto remote = snapshot.value("remote").toArray();
    QCOMPARE(remote.size(), 1);
    const auto item = remote.first().toObject();
    QCOMPARE(item.value("id").toString(),
             QString("org.lunadash.kde-behavior"));
    QCOMPARE(item.value("targets").toArray().size(), 3);
    const auto source = item.value("sourceUrl").toString();
    QVERIFY2(source.startsWith(
                 "https://github.com/LuYishan-4/LunaDash-Plugins/"),
             qPrintable(source));
    QVERIFY(snapshot.value("storeSupported").toBool());
    QVERIFY(!snapshot.value("storeLoading").toBool());
  }

  void configurationRecovery() {
    QString error;
    QVERIFY(!saveExtensionConfiguration(
        R"({"schemaVersion":1,"builtins":{},"plugins":{"absent":{"enabled":false,"settings":{},"mode":"replace","unexpected":1}}})",
        &error));
    auto document = config("absent", false, "replace");
    save(document);
    QCOMPARE(readExtensionConfiguration(), document);
    const auto previous = readExtensionConfiguration();
    QVERIFY(!saveExtensionConfiguration(QByteArray(24577, ' '), &error));
    QCOMPARE(readExtensionConfiguration(), previous);
  }
  void exclusiveTargetOwnership() {
    const auto first = root + "/org.example.panel-a";
    const auto second = root + "/org.example.panel-b";
    QVERIFY(createQmlPlugin(root, "org.example.panel-a", "panel"));
    QVERIFY(createQmlPlugin(root, "org.example.panel-b", "panel"));

    const QJsonObject conflicting{
        {"schemaVersion", 1},
        {"builtins", QJsonObject{}},
        {"plugins",
         QJsonObject{
             {"org.example.panel-a",
              QJsonObject{{"enabled", true},
                          {"mode", "augment"},
                          {"settings", QJsonObject{}}}},
             {"org.example.panel-b",
              QJsonObject{{"enabled", true},
                          {"mode", "augment"},
                          {"settings", QJsonObject{}}}}}}};
    QString error;
    QVERIFY(!saveExtensionConfiguration(
        QJsonDocument(conflicting).toJson(), &error));
    QVERIFY2(error.contains("Only one plugin can be enabled for: panel"),
             qPrintable(error));

    PluginManager manager;
    manager.refresh();
    const auto installed = manager.snapshot().value("installed").toArray();
    int panelPlugins = 0;
    int available = 0;
    int conflicts = 0;
    for (const auto &value : installed) {
      const auto item = value.toObject();
      if (item.value("target").toString() != "panel" ||
          !item.value("id").toString().startsWith("org.example.panel-"))
        continue;
      ++panelPlugins;
      if (item.value("available").toBool())
        ++available;
      if (item.value("error")
              .toString()
              .contains("single-owner target"))
        ++conflicts;
    }
    QCOMPARE(panelPlugins, 2);
    QCOMPARE(available, 1);
    QCOMPARE(conflicts, 1);

    QVERIFY(QDir(first).removeRecursively());
    QVERIFY(QDir(second).removeRecursively());
  }

  void externalMultiTarget() {
    const auto source =
        QString::fromLocal8Bit(qgetenv("LUNADASH_EXTERNAL_PLUGIN_DIR"));
    if (source.isEmpty() || !QFile::exists(source + "/metadata.json"))
      QSKIP("External multi-target plugin package was not supplied");

    const auto directory = root + "/org.lunadash.kde-behavior";
    QVERIFY(copyPlugin(source, directory));
    const auto descriptors =
        readPluginMetadataTargets(directory + "/metadata.json");
    QCOMPARE(descriptors.size(), 3);

    QSet<QString> targets;
    for (const auto &descriptor : descriptors) {
      QVERIFY2(descriptor.error.isEmpty(), qPrintable(descriptor.error));
      targets.insert(descriptor.target);
      QVERIFY(!descriptor.enabled);
    }
    QCOMPARE(targets,
             QSet<QString>({"panel", "window-rules", "window-animation"}));

    const QJsonObject targetConfigs{
        {"panel",
         QJsonObject{
             {"enabled", true},
             {"mode", "replace"},
             {"settings",
              QJsonObject{{"compact", true},
                          {"showLabels", false},
                          {"showWorkspaceSwitcher", true}}}}},
        {"window-rules",
         QJsonObject{
             {"enabled", true},
             {"mode", "replace"},
             {"settings",
              QJsonObject{{"workspacePolicy", "first"},
                          {"maximizeNewWindows", true}}}}},
        {"window-animation",
         QJsonObject{
             {"enabled", true},
             {"mode", "replace"},
             {"settings",
              QJsonObject{{"duration", 333},
                          {"enterOffset", 7},
                          {"focusOpacity", 0.91},
                          {"exitScale", 0.95},
                          {"easing", "outQuint"}}}}}};
    const QJsonObject configuration{
        {"schemaVersion", 1},
        {"builtins", QJsonObject{}},
        {"plugins",
         QJsonObject{{"org.lunadash.kde-behavior",
                      QJsonObject{{"enabled", true},
                                  {"targets", targetConfigs}}}}}};
    save(configuration);

    PluginManager manager;
    manager.refresh();
    const auto snapshot = manager.snapshot();
    const auto installed = snapshot.value("installed").toArray();
    QCOMPARE(installed.size(), 3);
    for (const auto &value : installed) {
      const auto item = value.toObject();
      QCOMPARE(item.value("id").toString(),
               QString("org.lunadash.kde-behavior"));
      QVERIFY2(item.value("available").toBool(),
               qPrintable(item.value("error").toString()));
    }

    const auto panel = std::find_if(
        installed.begin(), installed.end(), [](const QJsonValue &value) {
          return value.toObject().value("target").toString() == "panel";
        });
    QVERIFY(panel != installed.end());
    QVERIFY(panel->toObject().value("settings").toObject().value("compact").toBool());
    QVERIFY(!panel->toObject()
                 .value("settings")
                 .toObject()
                 .value("showLabels")
                 .toBool());

    const auto rule = initialWindowRule(
        manager, {{"appId", "org.example.App"}, {"title", "Example"}},
        4, false, 10);
    QCOMPARE(rule.value("workspace").toInt(), 0);
    QVERIFY(rule.value("maximized").toBool());

    const QJsonObject preferences{
        {"animations", true}, {"animationDuration", 200}};
    const auto profile = windowAnimationProfile(
        manager, preferences, windowTemplateForKey("tiling"));
    QCOMPARE(profile.value("duration").toInt(), 333);
    QCOMPARE(profile.value("enterOffset").toInt(), 7);
    QCOMPARE(profile.value("easing").toString(), QString("outQuint"));

    auto document = readExtensionConfiguration();
    auto packages = document.value("plugins").toObject();
    auto package =
        packages.value("org.lunadash.kde-behavior").toObject();
    auto runtimeTargets = package.value("targets").toObject();
    auto animation = runtimeTargets.value("window-animation").toObject();
    animation["enabled"] = false;
    runtimeTargets["window-animation"] = animation;
    package["targets"] = runtimeTargets;
    packages["org.lunadash.kde-behavior"] = package;
    document["plugins"] = packages;
    save(document);
    manager.refresh();
    QCOMPARE(windowAnimationProfile(
                 manager, preferences, windowTemplateForKey("tiling"))
                 .value("duration")
                 .toInt(),
             200);
    QCOMPARE(initialWindowRule(
                 manager, {{"appId", "org.example.App2"}}, 3, false, 10)
                 .value("workspace")
                 .toInt(),
             0);
  }

  void nativeLifecycle() {
    const auto source =
        QCoreApplication::applicationDirPath() + "/plugins/org.ludash.fade";
    if (!QFile::exists(source + "/libludash-fade.so"))
      QSKIP("Native examples disabled in this build");
    const auto directory = root + "/org.ludash.fade";
    QVERIFY(copyPlugin(source, directory));
    auto descriptor = readPluginMetadata(directory + "/metadata.json");
    QVERIFY2(descriptor.error.isEmpty(), qPrintable(descriptor.error));
    QVERIFY(!descriptor.enabled);

    PluginManager manager;
    const QJsonObject prefs{{"animations", true}, {"animationDuration", 200}};
    const auto &tilingTemplate = windowTemplateForKey("tiling");
    QCOMPARE(windowAnimationProfile(manager, prefs, tilingTemplate)
                 .value("duration")
                 .toInt(),
             200);

    save(config(descriptor.id, true, "replace", {{"duration", 310}}));
    manager.refresh();
    QCOMPARE(windowAnimationProfile(manager, prefs, tilingTemplate)
                 .value("duration")
                 .toInt(),
             310);

    save(config(descriptor.id, true, "augment", {{"duration", 150}}));
    manager.refresh();
    QCOMPARE(windowAnimationProfile(manager, prefs, tilingTemplate)
                 .value("duration")
                 .toInt(),
             150);

    auto noMotion = prefs;
    noMotion["animations"] = false;
    QCOMPARE(windowAnimationProfile(manager, noMotion, tilingTemplate)
                 .value("duration")
                 .toInt(),
             0);

    // Native packages are loaded directly. Disable first so the shared library
    // is unloaded before replacing the installed binary.
    save(config(descriptor.id, false, "augment"));
    manager.refresh();
    QCOMPARE(windowAnimationProfile(manager, prefs, tilingTemplate)
                 .value("duration")
                 .toInt(),
             200);

    const auto library = directory + "/libludash-fade.so";
    QFile file(library);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("incomplete build");
    file.close();

    save(config(descriptor.id, true, "augment", {{"duration", 150}}));
    manager.refresh();
    QCOMPARE(windowAnimationProfile(manager, prefs, tilingTemplate)
                 .value("duration")
                 .toInt(),
             200);

    QVERIFY(QFile::remove(library));
    QVERIFY(QFile::copy(source + "/libludash-fade.so", library));
    manager.reportError(descriptor.id, "");
    QCOMPARE(windowAnimationProfile(manager, prefs, tilingTemplate)
                 .value("duration")
                 .toInt(),
             150);

    save(config(descriptor.id, false, "augment"));
    manager.refresh();
    QCOMPARE(windowAnimationProfile(manager, prefs, tilingTemplate)
                 .value("duration")
                 .toInt(),
             200);
  }
  void receiptAndPathValidation() {
    const auto directory = root + "/org.ludash.fade";
    if (!QFile::exists(directory + "/metadata.json"))
      QSKIP("Native examples disabled in this build");
    QFile manifest(directory + "/metadata.json");
    QVERIFY(manifest.open(QIODevice::ReadOnly));
    const auto original = manifest.readAll();
    manifest.close();
    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    auto object = QJsonDocument::fromJson(original).object();
    object["entry"] = "../libexternal.so";
    manifest.write(QJsonDocument(object).toJson());
    manifest.close();
    QVERIFY(!readPluginMetadata(manifest.fileName()).error.isEmpty());
    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    manifest.write(original);
    manifest.close();
    QVERIFY(readPluginMetadata(manifest.fileName()).error.isEmpty());
  }
  void stackingExample() {
    const auto source = QCoreApplication::applicationDirPath() +
                        "/plugins/org.lunadash.stacking-windows";
    if (!QFile::exists(source + "/liblunadash-stacking-windows.so"))
      QSKIP("Native examples disabled in this build");
    const auto directory = root + "/org.lunadash.stacking-windows";
    QVERIFY(copyPlugin(source, directory));
    save(config("org.lunadash.stacking-windows", true, "replace",
                {{"cascadeStep", 32}}));
    PluginManager manager;
    manager.refresh();
    QCOMPARE(manager.windowTemplateKey(), QString("stacking"));
    const auto &stackingTemplate =
        windowTemplateForKey(manager.windowTemplateKey());
    auto layout = createWindowLayout(stackingTemplate);
    layout->setPlacementFilter(
        [&](auto workspace, auto area, const auto &windows) {
          return pluginWindowPlacements(manager, stackingTemplate, workspace,
                                        area, windows);
        });
    const QRect area(10, 40, 1200, 800);
    layout->insert(0, 1, QSize(800, 600));
    layout->insert(0, 2, QSize(800, 600));
    auto placed = layout->layout(0, area);
    QCOMPARE(placed[0].geometry.topLeft(), area.topLeft());
    QCOMPARE(placed[1].geometry.topLeft(), area.topLeft() + QPoint(32, 32));
    QVERIFY(placed[0].geometry.intersects(placed[1].geometry));
    layout->performAction(
        "move-by",
        {{"window", 1},
         {"dx", 70},
         {"dy", 50},
         {"area", QJsonObject{{"x", area.x()},
                              {"y", area.y()},
                              {"width", area.width()},
                              {"height", area.height()}}}});
    const auto moved = layout->layout(0, area);
    QCOMPARE(moved[0].geometry.topLeft(), area.topLeft() + QPoint(70, 50));
    QCOMPARE(moved[1].geometry, placed[1].geometry);
    save(config("org.lunadash.stacking-windows", false, "replace"));
    manager.refresh();
    QCOMPARE(manager.windowTemplateKey(), QString("tiling"));
  }
};
} // namespace LunaDash
QTEST_GUILESS_MAIN(LunaDash::PluginTests)
#include "PluginTests.moc"
