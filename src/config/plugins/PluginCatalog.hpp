#pragma once
#include <QList>
#include <QString>

namespace LunaDash {
struct PluginDescriptor {
  QString id;
  QString name;
  QString description;
  QString version;
  QString author;
  QString icon;
  QString type;
  QString entryPath;
  QString libraryPath;
  QString metadataPath;
  QString error;
  bool enabled = false;
};
PluginDescriptor readPluginMetadata(const QString &metadataPath);
QList<PluginDescriptor> discoverPlugins();
} // namespace LunaDash
