#include <LuDash/plugins/PluginManager.h>
#include <LuDash/plugins/CompositorPlugin.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSet>
#include <QUrl>
#include <algorithm>

namespace LuDash {
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
  if (root.isEmpty() || child.isEmpty() || QFileInfo(child).absolutePath() != root)
    return {};
  return child;
}
} // namespace

PluginDescriptor readPluginMetadata(const QString &path) {
  PluginDescriptor result;
  result.metadataPath = path;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) {
    result.error = "Unreadable or oversized metadata";
    return result;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    result.error = "Invalid JSON metadata";
    return result;
  }

  const auto metadata = document.object();
  const auto locale = QSettings()
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
                        : info.value("Authors").toArray().first().toObject().value("Name").toString();
    result.icon = info.value("Icon").toString("applications-system");
    if (api.value("ApiVersion").toInt() != 1 ||
        api.value("Type").toString() != "WindowEffect") {
      result.error = "Unsupported LuDash plugin API or type";
      return result;
    }
    result.type = "effect";
    result.libraryPath = canonicalChild(directory, api.value("Library").toString());
    if (result.libraryPath.isEmpty()) {
      result.error = "Missing library or library path escapes plugin directory";
      return result;
    }
  } else {
    if (metadata.value("schemaVersion").toInt() != 1) {
      result.error = "Unsupported plugin manifest schema";
      return result;
    }
    result.id = metadata.value("id").toString();
    result.version = metadata.value("version").toString();
    result.name = localized(metadata, "name", locale);
    result.description = localized(metadata, "description", locale);
    const auto author = metadata.value("author");
    result.author = author.isObject() ? author.toObject().value("name").toString()
                                      : author.toString();
    result.icon = metadata.value("icon").toString("applications-system");
    result.type = metadata.value("type").toString().toLower();
    enabledByDefault = metadata.value("enabledByDefault").toBool(false);
    if (result.type == "qml") {
      result.entryPath = canonicalChild(directory, metadata.value("entry").toString());
      if (result.entryPath.isEmpty() || !result.entryPath.endsWith(".qml")) {
        result.error = "QML plugin entry must be a QML file inside its directory";
        return result;
      }
    } else if (result.type == "effect") {
      result.libraryPath = canonicalChild(directory, metadata.value("entry").toString());
      if (result.libraryPath.isEmpty()) {
        result.error = "Effect plugin entry must be a library inside its directory";
        return result;
      }
    } else {
      result.error = "Plugin type must be qml or effect";
      return result;
    }
  }

  static const QRegularExpression validId("^[a-zA-Z0-9][a-zA-Z0-9._-]+$");
  if (!validId.match(result.id).hasMatch() || result.name.isEmpty() ||
      result.version.isEmpty()) {
    result.error = "Missing or invalid plugin identity";
    return result;
  }
  result.enabled = QSettings().value("plugins/" + result.id + "/enabled",
                                     enabledByDefault).toBool();
  return result;
}

QList<PluginDescriptor> discoverPlugins() {
  QStringList roots;
  roots << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../qml/plugins");
  for (const auto &path :
       QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
    roots << path + "/lunadash/shell/plugins";
    roots << path + "/ludash/plugins";
  }
  roots << QCoreApplication::applicationDirPath() + "/plugins";

  QList<PluginDescriptor> result;
  QSet<QString> seen;
  for (const auto &root : roots) {
    const QDir directory(root);
    for (const auto &name : directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      auto plugin = readPluginMetadata(directory.filePath(name + "/metadata.json"));
      if (!plugin.id.isEmpty() && seen.contains(plugin.id))
        continue;
      if (!plugin.id.isEmpty())
        seen.insert(plugin.id);
      result << plugin;
    }
  }
  return result;
}

PluginManager::PluginManager(QObject *parent) : QObject(parent) {}

void PluginManager::loadEnabled() {
  errors_.clear();
  for (const auto &descriptor : discoverPlugins()) {
    if (!descriptor.error.isEmpty()) {
      errors_ << (descriptor.id.isEmpty() ? descriptor.error
                                          : descriptor.id + ": " + descriptor.error);
      continue;
    }
    if (!descriptor.enabled || descriptor.type != "effect")
      continue;
    auto *loader = new QPluginLoader(descriptor.libraryPath, this);
    const auto embedded = loader->metaData();
    if (embedded.value("IID").toString() != LUDASH_COMPOSITOR_PLUGIN_IID) {
      errors_ << descriptor.id + ": metadata/IID mismatch";
      delete loader;
      continue;
    }
    auto *plugin = qobject_cast<CompositorPlugin *>(loader->instance());
    if (!plugin) {
      errors_ << descriptor.id + ": " + loader->errorString();
      delete loader;
      continue;
    }
    loaders_ << loader;
    plugins_ << plugin;
  }
}

QJsonObject PluginManager::snapshot() const {
  QJsonArray installed;
  for (const auto &descriptor : discoverPlugins()) {
    QJsonObject item{{"id", descriptor.id},
                     {"name", descriptor.name},
                     {"description", descriptor.description},
                     {"version", descriptor.version},
                     {"author", descriptor.author},
                     {"icon", descriptor.icon},
                     {"type", descriptor.type},
                     {"enabled", descriptor.enabled},
                     {"error", descriptor.error},
                     {"restartRequired", descriptor.type == "effect"}};
    if (descriptor.type == "qml" && !descriptor.entryPath.isEmpty())
      item.insert("entry", QUrl::fromLocalFile(descriptor.entryPath).toString());
    installed.append(item);
  }
  QJsonArray errors;
  for (const auto &error : errors_)
    errors.append(error);
  return {{"installed", installed}, {"errors", errors}, {"storeSupported", false}};
}

bool PluginManager::setEnabled(const QString &id, bool enabled, QString *error) {
  const auto plugins = discoverPlugins();
  const auto found = std::find_if(plugins.begin(), plugins.end(),
                                  [&id](const auto &plugin) {
                                    return plugin.id == id && plugin.error.isEmpty();
                                  });
  if (found == plugins.end()) {
    if (error)
      *error = "Unknown or invalid plugin.";
    return false;
  }
  QSettings settings;
  settings.setValue("plugins/" + id + "/enabled", enabled);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error)
      *error = "Could not save plugin state.";
    return false;
  }
  return true;
}

void PluginManager::windowOpened(QQuickItem *frame) {
  for (auto *plugin : plugins_)
    plugin->windowOpened(frame);
}
void PluginManager::windowFocused(QQuickItem *frame) {
  for (auto *plugin : plugins_)
    plugin->windowFocused(frame);
}
QStringList PluginManager::errors() const { return errors_; }
} // namespace LuDash
