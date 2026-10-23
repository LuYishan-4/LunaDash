#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <vector>

namespace LunaDash {
struct PluginDescriptor {
  QString id;
  QString name;
  QString description;
  QString version;
  QString author;
  QString icon;
  QStringList tags;
  QString type;
  QString target;
  QString mode = "augment";
  int schemaVersion = 1;
  QJsonObject manifest;
  QJsonObject implementation;
  QJsonObject settingsSchema;
  QJsonObject settings;
  QJsonObject shaders;
  QString entryPath;
  QString libraryPath;
  QString metadataPath;
  QString error;
  bool enabled = false;
};
PluginDescriptor readPluginMetadata(const QString &metadataPath,
                                    bool applyConfiguration = true);
std::vector<PluginDescriptor> readPluginMetadataTargets(
    const QString &metadataPath, bool applyConfiguration = true);
QString pluginInstanceId(const PluginDescriptor &plugin);
std::vector<PluginDescriptor> discoverPlugins();
QJsonObject pluginDescriptorJson(const PluginDescriptor &plugin);
QString pluginStoreFingerprint(const QJsonObject &storeItem);
bool pluginMatchesStore(const PluginDescriptor &plugin,
                        const QJsonObject &storeItem);
} // namespace LunaDash
