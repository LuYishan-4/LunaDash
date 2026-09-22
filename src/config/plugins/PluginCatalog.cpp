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
} // namespace

PluginDescriptor readPluginMetadata(const QString &path,
                                    bool applyConfiguration) {
  PluginDescriptor result;
  result.metadataPath = path;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) {
    result.error = "Unreadable or oversized metadata";
    return result;
  }
  QJsonParseError parseError;
  const auto metadataBytes = file.readAll();
  const auto document = QJsonDocument::fromJson(metadataBytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    result.error = "Invalid JSON metadata";
    return result;
  }

  const auto metadata = document.object();
  result.manifest = metadata;
  result.schemaVersion = metadata.value("schemaVersion").toInt(1);
  const auto locale =
      QSettings()
          .value("appearance/language", QLocale::system().name())
          .toString();
  const QDir directory = QFileInfo(file).absoluteDir();
  bool enabledByDefault = false;

  if (metadata.contains("KPlugin")) {
    const auto info = metadata.value("KPlugin").toObject();
    const auto api = metadata.value("LuDash").toObject();
    result.id = info.value("Id").toString();
    result.version = info.value("Version").toString();
    result.name = localized(info, "Name", locale);
    result.description = localized(info, "Description", locale);
    result.author = info.value("Authors").toArray().isEmpty()
                        ? QString()
                        : info.value("Authors")
                              .toArray()
                              .first()
                              .toObject()
                              .value("Name")
                              .toString();
    result.icon = info.value("Icon").toString("applications-system");
    if (api.value("ApiVersion").toInt() != 1 ||
        api.value("Type").toString() != "WindowEffect") {
      result.error = "Unsupported LuDash plugin API or type";
      return result;
    }
    result.type = "effect";
    result.libraryPath =
        canonicalChild(directory, api.value("Library").toString());
    if (result.libraryPath.isEmpty()) {
      result.error = "Missing library or library path escapes plugin directory";
      return result;
    }
  } else {
    if (result.schemaVersion != 1 && result.schemaVersion != 2) {
      result.error = "Unsupported plugin manifest schema";
      return result;
    }
    result.id = metadata.value("id").toString();
    result.version = metadata.value("version").toString();
    result.name = localized(metadata, "name", locale);
    result.description = localized(metadata, "description", locale);
    const auto author = metadata.value("author");
    result.author = author.isObject()
                        ? author.toObject().value("name").toString()
                        : author.toString();
    result.icon = metadata.value("icon").toString("applications-system");
    const auto tagsValue = metadata.value("tags");
    if (tagsValue.isArray()) {
      QSet<QString> seenTags;
      for (const auto &value : tagsValue.toArray()) {
        const auto tag = value.toString().trimmed();
        if (tag.isEmpty() || tag.size() > 32 || seenTags.contains(tag))
          continue;
        seenTags.insert(tag);
        result.tags << tag;
      }
    }
    result.type = metadata.value("type").toString().toLower();
    if (result.type == "qml")
      result.type = "quickshell";
    result.target = metadata.value("target").toString("desktop-widgets");
    result.mode = metadata.value("mode").toString("augment");
    if (result.schemaVersion == 2) {
      QFile receipt(canonicalChild(directory, ".lunadash-sdk.json"));
      if (!receipt.open(QIODevice::ReadOnly) || receipt.size() > 4096 ||
          QJsonDocument::fromJson(receipt.readAll())
                  .object()
                  .value("metadataSha256")
                  .toString() !=
              QString::fromLatin1(QCryptographicHash::hash(
                                      metadataBytes, QCryptographicHash::Sha256)
                                      .toHex())) {
        result.error = "Missing or stale LunaDash CMake SDK build receipt; "
                       "rebuild the plugin";
        return result;
      }
      const auto target = extensionTarget(result.target);
      const auto sdk = metadata.value("sdk").toObject();
      if (sdk.value("name").toString() != "LunaDash" ||
          sdk.value("apiVersion").toInt() != 2 || target.isEmpty() ||
          !target.value("types").toArray().contains(result.type) ||
          !metadata.contains("target") || !metadata.contains("mode") ||
          !QStringList{"replace", "augment"}.contains(result.mode)) {
        result.error = "Invalid SDK, target, type or composition mode";
        return result;
      }
      if (!metadata.value("settings").isObject()) {
        result.error = "Schema 2 requires a settings schema object";
        return result;
      }
      if (metadata.contains("tags") &&
          (!metadata.value("tags").isArray() ||
           metadata.value("tags").toArray().size() > 12 ||
           result.tags.size() != metadata.value("tags").toArray().size())) {
        result.error = "Plugin tags must be unique non-empty strings up to 32 characters";
        return result;
      }
      if (!safeLeaf(result.icon)) {
        result.error = "Plugin icon must be a theme name or local image filename";
        return result;
      }
      if (packagedImageIcon(result.icon)) {
        const auto iconPath = canonicalChild(directory, result.icon);
        if (iconPath.isEmpty()) {
          result.error = "Plugin icon is missing or escapes the plugin directory";
          return result;
        }
        result.icon = QUrl::fromLocalFile(iconPath).toString();
      }
      result.settingsSchema = metadata.value("settings").toObject();
      const auto windowTemplate =
          metadata.value("windowTemplate").toString("tiling");
      if ((metadata.contains("windowTemplate") &&
           (result.type != "effect" || result.target != "window-layout")) ||
          metadata.contains("layoutMode") ||
          !QStringList{"tiling", "stacking"}.contains(windowTemplate) ||
          (windowTemplate == "stacking" && result.mode != "replace")) {
        result.error =
            "windowTemplate stacking requires a window-layout replacement";
        return result;
      }
      result.settings = extensionDefaults(result.settingsSchema);
      if (!Settings::validatePluginSchema(result.settingsSchema, &result.error) ||
          !Settings::validateValues(result.settingsSchema, result.settings,
                                    &result.error))
        return result;
    }
    enabledByDefault = metadata.value("enabledByDefault").toBool(false);
    if (result.type == "quickshell") {
      result.entryPath =
          canonicalChild(directory, metadata.value("entry").toString());
      if (result.entryPath.isEmpty() || !result.entryPath.endsWith(".qml")) {
        result.error =
            "QML plugin entry must be a QML file inside its directory";
        return result;
      }
    } else if (result.type == "effect") {
      result.libraryPath =
          canonicalChild(directory, metadata.value("entry").toString());
      if (result.libraryPath.isEmpty()) {
        result.error =
            "Effect plugin entry must be a library inside its directory";
        return result;
      }
    } else if (result.type == "opengl" && result.schemaVersion == 2) {
      const auto shaders = metadata.value("shaders").toObject();
      for (const auto &stage : {QString("vertex"), QString("fragment")}) {
        const auto source = shaders.value(stage).toString();
        const auto path = canonicalChild(directory, source);
        const auto baked = canonicalChild(directory, source + ".qsb");
        const auto extension = stage == "vertex" ? ".vert" : ".frag";
        if (path.isEmpty() || baked.isEmpty() || !source.endsWith(extension)) {
          result.error = "OpenGL plugins require local .vert/.frag sources and "
                         "SDK-built .qsb files";
          return result;
        }
        result.shaders[stage] = QUrl::fromLocalFile(baked).toString();
      }
    } else {
      result.error = "Plugin type must be quickshell, effect or opengl";
      return result;
    }
  }

  // The old Qt Quick effect ABI never drove the active wlroots scene. Do not
  // load it and advertise a successful effect: require rebuilding with SDK 2.
  if (result.type == "effect" && result.schemaVersion != 2) {
    result.error = "Legacy native effect: rebuild with LunaDash Plugin SDK 2";
    return result;
  }

  static const QRegularExpression validId("^[a-zA-Z0-9][a-zA-Z0-9._-]+$");
  if (!validId.match(result.id).hasMatch() || result.name.isEmpty() ||
      result.version.isEmpty()) {
    result.error = "Missing or invalid plugin identity";
    return result;
  }
  result.enabled =
      QSettings()
          .value("plugins/" + result.id + "/enabled",
                 result.type == "effect" ? false : enabledByDefault)
          .toBool();
  if (!applyConfiguration)
    return result;
  const auto config = readExtensionConfiguration()
                          .value("plugins")
                          .toObject()
                          .value(result.id)
                          .toObject();
  if (config.contains("enabled"))
    result.enabled = config.value("enabled").toBool(false);
  if (config.contains("mode"))
    result.mode = config.value("mode").toString();
  const auto configuredWindowTemplate =
      result.manifest.value("windowTemplate").toString("tiling");
  if (configuredWindowTemplate == "stacking" && result.mode != "replace") {
    result.error = "Stacking window template must run as Plugin only";
    result.enabled = false;
    return result;
  }
  const auto settings = config.value("settings").toObject();
  if (!QStringList{"replace", "augment"}.contains(result.mode) ||
      !Settings::validatePluginSchema(result.settingsSchema, &result.error) ||
      !Settings::validateValues(result.settingsSchema, settings,
                                &result.error)) {
    if (result.error.isEmpty())
      result.error = "Invalid plugin composition mode";
    result.enabled = false;
    return result;
  }
  for (auto it = settings.begin(); it != settings.end(); ++it)
    result.settings[it.key()] = it.value();
  return result;
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
       plugin.manifest.value("windowTemplate").toString("tiling")},
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
    roots << path + "/lunadash/plugins";
    roots << path + "/lunadash/shell/plugins";
    roots << path + "/ludash/plugins";
  }
  roots << QCoreApplication::applicationDirPath() + "/plugins";

  QList<PluginDescriptor> result;
  QSet<QString> seen;
  for (const auto &root : roots) {
    const QDir directory(root);
    for (const auto &name :
         directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      auto plugin =
          readPluginMetadata(directory.filePath(name + "/metadata.json"));
      if (!plugin.id.isEmpty() && seen.contains(plugin.id))
        continue;
      if (!plugin.id.isEmpty())
        seen.insert(plugin.id);
      result << plugin;
    }
  }
  std::sort(result.begin(), result.end(),
            [](const auto &a, const auto &b) { return a.id < b.id; });
  return result;
}

} // namespace LunaDash
