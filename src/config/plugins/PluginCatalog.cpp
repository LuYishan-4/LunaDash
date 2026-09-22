#include "config/plugins/PluginCatalog.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "core/settings/SettingsSchema.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>

namespace LunaDash {
namespace {
QString localized(const QJsonObject &object, const QString &key,
                  const QString &locale) {
  return object.value(key + "[" + locale + "]")
      .toString(object.value(key).toString());
}

bool safeLeaf(const QString &value) {
  return !value.isEmpty() && !value.contains('/') && !value.contains('\\') &&
         value != "." && value != "..";
}

QString canonicalChild(const QDir &directory, const QString &leaf) {
  if (!safeLeaf(leaf))
    return {};
  const QString root = QFileInfo(directory.absolutePath()).canonicalFilePath();
  const QString child = QFileInfo(directory.filePath(leaf)).canonicalFilePath();
  if (root.isEmpty() || child.isEmpty() ||
      QFileInfo(child).absolutePath() != root)
    return {};
  return child;
}

bool packagedImageIcon(const QString &value) {
  static const QRegularExpression suffix(
      R"(\.(png|jpe?g|webp|svg)$)",
      QRegularExpression::CaseInsensitiveOption);
  return suffix.match(value).hasMatch();
}

QJsonObject targetConfiguration(const PluginDescriptor &plugin,
                                const QJsonObject &packageConfig) {
  const auto targets = packageConfig.value("targets").toObject();
  if (targets.contains(plugin.target))
    return targets.value(plugin.target).toObject();
  if (plugin.manifest.value("targets").isArray())
    return {};
  return packageConfig;
}

PluginDescriptor errorDescriptor(const QString &path, const QString &message) {
  PluginDescriptor result;
  result.metadataPath = path;
  result.error = message;
  return result;
}
} // namespace

QString pluginInstanceId(const PluginDescriptor &plugin) {
  return plugin.id + "@" + plugin.target;
}

QList<PluginDescriptor> readPluginMetadataTargets(const QString &path,
                                                  bool applyConfiguration) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
    return {errorDescriptor(path, "Unreadable or oversized metadata")};

  QJsonParseError parseError;
  const auto metadataBytes = file.readAll();
  const auto document = QJsonDocument::fromJson(metadataBytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
    return {errorDescriptor(path, "Invalid JSON metadata")};

  const auto metadata = document.object();
  const int schemaVersion = metadata.value("schemaVersion").toInt(1);
  const auto locale =
      QSettings().value("appearance/language", QLocale::system().name()).toString();
  const QDir directory = QFileInfo(file).absoluteDir();

  PluginDescriptor base;
  base.metadataPath = path;
  base.manifest = metadata;
  base.schemaVersion = schemaVersion;

  if (metadata.contains("KPlugin")) {
    const auto info = metadata.value("KPlugin").toObject();
    const auto api = metadata.value("LuDash").toObject();
    base.id = info.value("Id").toString();
    base.version = info.value("Version").toString();
    base.name = localized(info, "Name", locale);
    base.description = localized(info, "Description", locale);
    base.author = info.value("Authors").toArray().isEmpty()
                      ? QString()
                      : info.value("Authors").toArray().first().toObject().value("Name").toString();
    base.icon = info.value("Icon").toString("applications-system");
    base.type = "effect";
    base.target = "window-animation";
    base.mode = "augment";
    base.implementation = metadata;
    base.libraryPath = canonicalChild(directory, api.value("Library").toString());
    base.error = "Legacy native effect: rebuild with LunaDash Plugin SDK 2";
    return {base};
  }

  if (schemaVersion != 1 && schemaVersion != 2)
    return {errorDescriptor(path, "Unsupported plugin manifest schema")};

  base.id = metadata.value("id").toString();
  base.version = metadata.value("version").toString();
  base.name = localized(metadata, "name", locale);
  base.description = localized(metadata, "description", locale);
  const auto author = metadata.value("author");
  base.author = author.isObject() ? author.toObject().value("name").toString()
                                  : author.toString();
  base.icon = metadata.value("icon").toString("applications-system");

  const auto tagsValue = metadata.value("tags");
  if (tagsValue.isArray()) {
    QSet<QString> seenTags;
    for (const auto &value : tagsValue.toArray()) {
      const auto tag = value.toString().trimmed();
      if (tag.isEmpty() || tag.size() > 32 || seenTags.contains(tag))
        continue;
      seenTags.insert(tag);
      base.tags << tag;
    }
  }

  static const QRegularExpression validId("^[a-zA-Z0-9][a-zA-Z0-9._-]+$");
  if (!validId.match(base.id).hasMatch() || base.name.isEmpty() ||
      base.version.isEmpty()) {
    base.error = "Missing or invalid plugin identity";
    return {base};
  }

  if (schemaVersion == 2) {
    const auto receiptPath = canonicalChild(directory, ".lunadash-sdk.json");
    QFile receipt(receiptPath);
    if (receiptPath.isEmpty() || !receipt.open(QIODevice::ReadOnly) ||
        receipt.size() > 4096 ||
        QJsonDocument::fromJson(receipt.readAll()).object().value("metadataSha256").toString() !=
            QString::fromLatin1(QCryptographicHash::hash(metadataBytes, QCryptographicHash::Sha256).toHex())) {
      base.error = "Missing or stale LunaDash CMake SDK build receipt; rebuild the plugin";
      return {base};
    }
    const auto sdk = metadata.value("sdk").toObject();
    if (sdk.value("name").toString() != "LunaDash" ||
        sdk.value("apiVersion").toInt() != 2) {
      base.error = "Invalid LunaDash Plugin SDK declaration";
      return {base};
    }
    if (!safeLeaf(base.icon)) {
      base.error = "Plugin icon must be a theme name or local image filename";
      return {base};
    }
    if (packagedImageIcon(base.icon)) {
      const auto iconPath = canonicalChild(directory, base.icon);
      if (iconPath.isEmpty()) {
        base.error = "Plugin icon is missing or escapes the plugin directory";
        return {base};
      }
      base.icon = QUrl::fromLocalFile(iconPath).toString();
    }
  }

  QJsonArray implementations;
  if (schemaVersion == 2 && metadata.value("targets").isArray()) {
    implementations = metadata.value("targets").toArray();
    if (implementations.isEmpty() || implementations.size() > 16) {
      base.error = "Plugin targets must contain between 1 and 16 entries";
      return {base};
    }
  } else {
    implementations.append(metadata);
  }

  const auto allConfig = applyConfiguration ? readExtensionConfiguration() : QJsonObject{};
  const auto packageConfig =
      allConfig.value("plugins").toObject().value(base.id).toObject();
  const bool packageDefault = metadata.value("enabledByDefault").toBool(false);
  const bool packageEnabled =
      applyConfiguration
          ? packageConfig.value("enabled").toBool(packageDefault)
          : packageDefault;

  QList<PluginDescriptor> results;
  QSet<QString> targetIds;
  for (const auto &value : implementations) {
    if (!value.isObject()) {
      auto invalid = base;
      invalid.error = "Plugin target entries must be objects";
      return {invalid};
    }
    const auto implementation = value.toObject();
    PluginDescriptor result = base;
    result.implementation = implementation;
    result.type = implementation.value("type").toString().toLower();
    if (result.type == "qml")
      result.type = "quickshell";
    result.target = implementation.value("target").toString(
        implementation.value("id").toString(
            metadata.value("target").toString("desktop-widgets")));
    result.mode = implementation.value("mode").toString("augment");

    if (result.target.isEmpty() || targetIds.contains(result.target)) {
      result.error = "Plugin targets must have unique non-empty target ids";
      return {result};
    }
    targetIds.insert(result.target);

    const auto target = extensionTarget(result.target);
    if (schemaVersion == 2 &&
        (target.isEmpty() ||
         !target.value("types").toArray().contains(result.type) ||
         !QStringList{"replace", "augment"}.contains(result.mode))) {
      result.error = "Invalid SDK target, type or composition mode";
      return {result};
    }

    result.settingsSchema = implementation.value("settings").toObject();
    result.settings = extensionDefaults(result.settingsSchema);
    if (schemaVersion == 2 &&
        (!implementation.value("settings").isObject() ||
         !Settings::validatePluginSchema(result.settingsSchema, &result.error) ||
         !Settings::validateValues(result.settingsSchema, result.settings,
                                   &result.error)))
      return {result};

    const auto windowTemplate =
        implementation.value("windowTemplate").toString("tiling");
    if ((implementation.contains("windowTemplate") &&
         (result.type != "effect" || result.target != "window-layout")) ||
        implementation.contains("layoutMode") ||
        !QStringList{"tiling", "stacking"}.contains(windowTemplate) ||
        (windowTemplate == "stacking" && result.mode != "replace")) {
      result.error =
          "windowTemplate stacking requires a window-layout replacement";
      return {result};
    }

    if (result.type == "quickshell") {
      result.entryPath =
          canonicalChild(directory, implementation.value("entry").toString());
      if (result.entryPath.isEmpty() || !result.entryPath.endsWith(".qml")) {
        result.error = "QML plugin entry must be a QML file inside its directory";
        return {result};
      }
    } else if (result.type == "effect") {
      result.libraryPath =
          canonicalChild(directory, implementation.value("entry").toString());
      if (result.libraryPath.isEmpty()) {
        result.error = "Effect plugin entry must be a library inside its directory";
        return {result};
      }
      if (schemaVersion != 2) {
        result.error = "Legacy native effect: rebuild with LunaDash Plugin SDK 2";
        return {result};
      }
    } else if (result.type == "opengl" && schemaVersion == 2) {
      const auto shaders = implementation.value("shaders").toObject();
      for (const auto &stage : {QString("vertex"), QString("fragment")}) {
        const auto source = shaders.value(stage).toString();
        const auto sourcePath = canonicalChild(directory, source);
        const auto baked = canonicalChild(directory, source + ".qsb");
        const auto extension = stage == "vertex" ? ".vert" : ".frag";
        if (sourcePath.isEmpty() || baked.isEmpty() ||
            !source.endsWith(extension)) {
          result.error =
              "OpenGL plugins require local .vert/.frag sources and SDK-built .qsb files";
          return {result};
        }
        result.shaders[stage] = QUrl::fromLocalFile(baked).toString();
      }
    } else {
      result.error = "Plugin type must be quickshell, effect or opengl";
      return {result};
    }

    const auto config = targetConfiguration(result, packageConfig);
    const bool targetEnabled =
        config.value("enabled").toBool(true);
    result.enabled = packageEnabled && targetEnabled;
    if (applyConfiguration && config.contains("mode"))
      result.mode = config.value("mode").toString();
    const auto changes = config.value("settings").toObject();
    if (applyConfiguration &&
        (!QStringList{"replace", "augment"}.contains(result.mode) ||
         !Settings::validatePluginSchema(result.settingsSchema, &result.error) ||
         !Settings::validateValues(result.settingsSchema, changes,
                                   &result.error))) {
      if (result.error.isEmpty())
        result.error = "Invalid plugin target configuration";
      result.enabled = false;
      return {result};
    }
    for (auto it = changes.begin(); it != changes.end(); ++it)
      result.settings[it.key()] = it.value();

    results.append(result);
  }

  return results;
}

PluginDescriptor readPluginMetadata(const QString &path,
                                    bool applyConfiguration) {
  const auto values = readPluginMetadataTargets(path, applyConfiguration);
  return values.isEmpty() ? errorDescriptor(path, "Plugin has no targets")
                          : values.first();
}

QJsonObject pluginDescriptorJson(const PluginDescriptor &plugin) {
  QJsonArray tags;
  QSet<QString> seen;
  const auto appendTag = [&](const QString &tag) {
    if (!tag.isEmpty() && !seen.contains(tag)) {
      seen.insert(tag);
      tags.append(tag);
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
       plugin.implementation.value("windowTemplate").toString("tiling")},
      {"enabled", plugin.enabled},
      {"error", plugin.error},
      {"settingsSchema", plugin.settingsSchema},
      {"settings", plugin.settings},
      {"shaders", plugin.shaders},
      {"entry", plugin.entryPath.isEmpty()
                    ? QString()
                    : QUrl::fromLocalFile(plugin.entryPath).toString()},
      {"restartRequired", false}};
}

QList<PluginDescriptor> discoverPlugins() {
  QStringList roots;
  for (const auto &path :
       QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
    // SDK 2 packages have one canonical location. Do not discover the old
    // lunadash/shell/plugins or ludash/plugins trees: stale files left by old
    // releases otherwise shadow Store packages with the same id.
    roots << path + "/lunadash/plugins";
  }
  roots << QCoreApplication::applicationDirPath() + "/plugins";

  QList<PluginDescriptor> result;
  QSet<QString> seenPackages;
  for (const auto &root : roots) {
    const QDir directory(root);
    for (const auto &name :
         directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      const auto values = readPluginMetadataTargets(
          directory.filePath(name + "/metadata.json"));
      if (values.isEmpty())
        continue;
      const auto id = values.first().id;
      if (!id.isEmpty() && seenPackages.contains(id))
        continue;
      if (!id.isEmpty())
        seenPackages.insert(id);
      result.append(values);
    }
  }
  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
    return a.id == b.id ? a.target < b.target : a.id < b.id;
  });
  return result;
}

} // namespace LunaDash
