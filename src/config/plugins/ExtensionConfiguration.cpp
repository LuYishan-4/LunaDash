#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "config/plugins/PluginCatalog.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

namespace LunaDash {
namespace {
bool fail(QString *error, const QString &message) {
  if (error)
    *error = message;
  return false;
}
bool allowed(const QJsonObject &object, const QStringList &keys) {
  for (auto it = object.begin(); it != object.end(); ++it)
    if (!keys.contains(it.key()))
      return false;
  return true;
}
} // namespace
QString extensionConfigurationPath() {
  return QStandardPaths::writableLocation(
             QStandardPaths::GenericConfigLocation) +
         "/LuDash/extensions.json";
}
QJsonObject readExtensionConfiguration(QString *error) {
  const QJsonObject empty{{"schemaVersion", 1},
                          {"builtins", QJsonObject{}},
                          {"plugins", QJsonObject{}}};
  QFile file(extensionConfigurationPath());
  if (!file.exists())
    return empty;
  if (!file.open(QIODevice::ReadOnly) || file.size() > 24576) {
    fail(error, "Could not read extensions.json (limit 24 KiB).");
    return empty;
  }
  const auto document = QJsonDocument::fromJson(file.readAll());
  const auto object = document.object();
  if (!document.isObject() || object.value("schemaVersion").toInt() != 1 ||
      !object.value("builtins").isObject() ||
      !object.value("plugins").isObject()) {
    fail(error, "Invalid extensions.json; built-in defaults are in use.");
    return empty;
  }
  return object;
}
QJsonObject configuredBuiltinSettings(const QString &target) {
  return configuredBuiltinSettings(target, readExtensionConfiguration());
}
QJsonObject configuredBuiltinSettings(const QString &target,
                                      const QJsonObject &document) {
  const auto schema = extensionTarget(target).value("settings").toObject();
  auto result = extensionDefaults(schema);
  const auto changes =
      document.value("builtins").toObject().value(target).toObject();
  if (validateExtensionSettings(schema, changes, nullptr))
    for (auto it = changes.begin(); it != changes.end(); ++it)
      result[it.key()] = it.value();
  return result;
}
bool saveExtensionConfiguration(const QByteArray &json, QString *error) {
  const auto document = QJsonDocument::fromJson(json);
  const auto root = document.object();
  if (json.size() > 24576 || !document.isObject() ||
      root.value("schemaVersion").toInt() != 1 ||
      !root.value("builtins").isObject() || !root.value("plugins").isObject() ||
      !allowed(root, {"schemaVersion", "builtins", "plugins"}))
    return fail(
        error,
        "Expected schemaVersion 1, builtins and plugins (24 KiB limit).");
  const auto builtins = root.value("builtins").toObject();
  for (auto it = builtins.begin(); it != builtins.end(); ++it) {
    const auto target = extensionTarget(it.key());
    if (target.isEmpty() || !it.value().isObject())
      return fail(error, "Unknown built-in target: " + it.key());
    if (!validateExtensionSettings(target.value("settings").toObject(),
                                   it.value().toObject(), error))
      return false;
  }
  const auto catalog = discoverPlugins();
  const auto plugins = root.value("plugins").toObject();
  for (auto it = plugins.begin(); it != plugins.end(); ++it) {
    if (!it.value().isObject())
      return fail(error, "Plugin configuration must be an object.");
    const auto config = it.value().toObject();
    if (!allowed(config, {"enabled", "mode", "settings"}) ||
        !config.value("enabled").isBool() ||
        !config.value("settings").isObject() ||
        !QStringList{"replace", "augment"}.contains(
            config.value("mode").toString()))
      return fail(error,
                  "Expected enabled, mode (replace/augment) and settings: " +
                      it.key());
    const auto found =
        std::find_if(catalog.begin(), catalog.end(),
                     [&](const auto &plugin) { return plugin.id == it.key(); });
    // Keep disabled settings for uninstalled plugins, for portable profiles.
    if (found == catalog.end()) {
      if (config.value("enabled").toBool())
        return fail(error, "Plugin is not installed: " + it.key());
      continue;
    }
    const auto descriptor = readPluginMetadata(found->metadataPath, false);
    if (config.value("enabled").toBool()) {
      if (!descriptor.error.isEmpty())
        return fail(error, descriptor.error);
      if (descriptor.manifest.value("layoutMode").toString() == "stacking" &&
          config.value("mode").toString() != "replace")
        return fail(error, "Stacking layout requires Plugin only; two "
                           "placement engines cannot own the same windows.");
    }
    if (!descriptor.error.isEmpty() && !config.value("enabled").toBool())
      continue;
    if (!validateExtensionSettings(descriptor.settingsSchema,
                                   config.value("settings").toObject(), error))
      return false;
  }
  // Omitted entries retain metadata/legacy defaults, so include them when
  // checking whether more than one replacement would own a target.
  QSet<QString> replacements;
  for (const auto &installed : catalog) {
    const auto descriptor = readPluginMetadata(installed.metadataPath, false);
    const auto config = plugins.value(descriptor.id).toObject();
    if (!descriptor.error.isEmpty() ||
        !config.value("enabled").toBool(descriptor.enabled) ||
        config.value("mode").toString(descriptor.mode) != "replace")
      continue;
    if (replacements.contains(descriptor.target))
      return fail(error, "Only one replacement can be enabled for: " +
                             descriptor.target);
    replacements.insert(descriptor.target);
  }
  const auto path = extensionConfigurationPath();
  if (QFileInfo(path).isSymLink())
    return fail(error, "Refusing to replace a symlinked extensions.json.");
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile file(path);
  const auto bytes = document.toJson(QJsonDocument::Compact);
  if (!file.open(QIODevice::WriteOnly) ||
      !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
      file.write(bytes) != bytes.size() || !file.commit())
    return fail(error, "Could not save extensions.json atomically.");
  return true;
}
} // namespace LunaDash
