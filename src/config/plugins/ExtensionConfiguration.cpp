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
QJsonObject effectiveTargetConfig(const PluginDescriptor &descriptor,
                                  const QJsonObject &package) {
  const auto targets = package.value("targets").toObject();
  if (targets.contains(descriptor.target))
    return targets.value(descriptor.target).toObject();
  if (!descriptor.manifest.value("targets").isArray())
    return package;
  return {};
}
bool effectiveEnabled(const PluginDescriptor &descriptor,
                      const QJsonObject &package) {
  const bool packageEnabled = package.value("enabled").toBool(
      descriptor.manifest.value("enabledByDefault").toBool(false));
  const auto target = effectiveTargetConfig(descriptor, package);
  return packageEnabled && target.value("enabled").toBool(true);
}
QString effectiveMode(const PluginDescriptor &descriptor,
                      const QJsonObject &package) {
  return effectiveTargetConfig(descriptor, package)
      .value("mode")
      .toString(descriptor.mode);
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
    const auto package = it.value().toObject();
    const bool multi = package.value("targets").isObject();
    if (multi) {
      if (!allowed(package, {"enabled", "targets"}) ||
          !package.value("enabled").isBool())
        return fail(error, "Multi-target plugin configuration expects enabled and targets: " + it.key());
      const auto targets = package.value("targets").toObject();
      for (auto targetIt = targets.begin(); targetIt != targets.end(); ++targetIt) {
        if (!targetIt.value().isObject())
          return fail(error, "Plugin target configuration must be an object.");
        const auto config = targetIt.value().toObject();
        if (!allowed(config, {"enabled", "mode", "settings"}) ||
            !config.value("enabled").isBool() ||
            !config.value("settings").isObject() ||
            !QStringList{"replace", "augment"}.contains(config.value("mode").toString()))
          return fail(error, "Expected enabled, mode and settings for " +
                                 it.key() + "/" + targetIt.key());
        const auto found = std::find_if(
            catalog.cbegin(), catalog.cend(), [&](const auto &plugin) {
              return plugin.id == it.key() && plugin.target == targetIt.key();
            });
        if (found == catalog.cend()) {
          if (package.value("enabled").toBool() &&
              config.value("enabled").toBool())
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
            !targetDescriptor->error.isEmpty())
          return fail(error, targetDescriptor == descriptor.cend()
                                 ? "Plugin target disappeared during validation."
                                 : targetDescriptor->error);
        if (!validateExtensionSettings(targetDescriptor->settingsSchema,
                                       config.value("settings").toObject(),
                                       error))
          return false;
      }
      continue;
    }

    if (!allowed(package, {"enabled", "mode", "settings"}) ||
        !package.value("enabled").isBool() ||
        !package.value("settings").isObject() ||
        !QStringList{"replace", "augment"}.contains(
            package.value("mode").toString()))
      return fail(error,
                  "Expected enabled, mode (replace/augment) and settings: " +
                      it.key());

    const auto matching = std::count_if(
        catalog.cbegin(), catalog.cend(),
        [&](const auto &plugin) { return plugin.id == it.key(); });
    if (matching == 0) {
      if (package.value("enabled").toBool())
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
    if (!descriptor.error.isEmpty() && package.value("enabled").toBool())
      return fail(error, descriptor.error);
    if (descriptor.error.isEmpty() &&
        !validateExtensionSettings(descriptor.settingsSchema,
                                   package.value("settings").toObject(), error))
      return false;
  }

  QHash<QString, QString> selected;
  QHash<QString, QString> replacements;
  QSet<QString> visitedPackages;
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
    if (descriptor == descriptors.cend() || !descriptor->error.isEmpty())
      continue;

    const auto package = plugins.value(descriptor->id).toObject();
    if (!effectiveEnabled(*descriptor, package))
      continue;

    const auto targetSpec = extensionTarget(descriptor->target);
    const auto owner = descriptor->id + "@" + descriptor->target;
    if (targetSpec.value("selection").toString("single") == "single") {
      if (selected.contains(descriptor->target) &&
          selected.value(descriptor->target) != owner)
        return fail(error, "Only one plugin can be enabled for: " +
                               descriptor->target);
      selected[descriptor->target] = owner;
    }

    if (effectiveMode(*descriptor, package) == "replace") {
      if (replacements.contains(descriptor->target) &&
          replacements.value(descriptor->target) != owner)
        return fail(error, "Only one replacement can be enabled for: " +
                               descriptor->target);
      replacements[descriptor->target] = owner;
    }
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
