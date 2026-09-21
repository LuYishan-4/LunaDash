#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "config/plugins/PluginCatalog.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include <QDir>
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
    QCoreApplication::setOrganizationName("LunaDashPluginTests");
    QCoreApplication::setApplicationName("Plugins");
    root = temporary.path() + "/data/lunadash/plugins";
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
