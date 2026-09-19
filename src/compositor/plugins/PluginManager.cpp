#include "compositor/plugins/PluginManager.hpp"
#include "compositor/plugins/CompositorPlugin.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>

namespace LunaDash {
PluginManager::PluginManager(QObject *parent) : QObject(parent) {}

void PluginManager::loadEnabled() {
  errors_.clear();
  for (const auto &descriptor : discoverPlugins()) {
    if (!descriptor.error.isEmpty()) {
      errors_ << (descriptor.id.isEmpty()
                      ? descriptor.error
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
      item.insert("entry",
                  QUrl::fromLocalFile(descriptor.entryPath).toString());
    installed.append(item);
  }
  QJsonArray errors;
  for (const auto &error : errors_)
    errors.append(error);
  return {
      {"installed", installed}, {"errors", errors}, {"storeSupported", false}};
}

bool PluginManager::setEnabled(const QString &id, bool enabled,
                               QString *error) {
  const auto plugins = discoverPlugins();
  const auto found =
      std::find_if(plugins.begin(), plugins.end(), [&id](const auto &plugin) {
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
} // namespace LunaDash
