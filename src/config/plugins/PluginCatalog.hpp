#pragma once
#include "core/json/JsonAccess.hpp"

#include <string>
#include <vector>

namespace LunaDash {
struct PluginDescriptor {
  std::string id;
  std::string name;
  std::string description;
  std::string version;
  std::string author;
  std::string icon;
  std::vector<std::string> tags;
  std::string type;
  std::string target;
  std::string mode = "augment";
  int schemaVersion = 1;
  Json manifest = Json::object();
  Json implementation = Json::object();
  Json settingsSchema = Json::object();
  Json settings = Json::object();
  std::string libraryPath;
  std::string metadataPath;
  std::string error;
  bool enabled = false;
};
PluginDescriptor readPluginMetadata(const std::string &metadataPath,
                                    bool applyConfiguration = true);
std::vector<PluginDescriptor> readPluginMetadataTargets(
    const std::string &metadataPath, bool applyConfiguration = true);
std::string pluginInstanceId(const PluginDescriptor &plugin);
std::vector<PluginDescriptor> discoverPlugins();
Json pluginDescriptorJson(const PluginDescriptor &plugin);
std::string pluginStoreFingerprint(const Json &storeItem);
bool pluginMatchesStore(const PluginDescriptor &plugin, const Json &storeItem);
} // namespace LunaDash
