#include "config/plugins/ExtensionRegistry.hpp"
#include "core/settings/SettingsSchema.hpp"

namespace LunaDash {
Json extensionTargets() {
  static const auto targets = [] {
    for (const char *path : {LUDASH_PLUGIN_TARGETS_SOURCE_FILE,
                             LUDASH_PLUGIN_TARGETS_INSTALL_FILE}) {
      auto document = readJsonFile(path);
      if (!document.is_discarded())
        return jsonAsArray(document);
    }
    return Json::array();
  }();
  return targets;
}
Json extensionTarget(const std::string &id) {
  for (const auto &value : extensionTargets())
    if (jsonString(value, "id") == id)
      return jsonAsObject(value);
  return Json::object();
}
Json extensionDefaults(const Json &schema) {
  return Settings::defaults(schema);
}
bool validateExtensionSettings(const Json &schema, const Json &settings,
                               std::string *error) {
  return Settings::validateSchema(schema, error) &&
         Settings::validateValues(schema, settings, error);
}
} // namespace LunaDash
