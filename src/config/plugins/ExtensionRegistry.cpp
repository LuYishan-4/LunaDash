#include "config/plugins/ExtensionRegistry.hpp"
#include <QFile>
#include <QJsonDocument>
#include "core/settings/SettingsSchema.hpp"

namespace LunaDash {
QJsonArray extensionTargets() {
  static const auto targets = [] {
    QFile file(QStringLiteral(LUDASH_PLUGIN_TARGETS_SOURCE_FILE));
    if (!file.open(QIODevice::ReadOnly)) {
      file.setFileName(QStringLiteral(LUDASH_PLUGIN_TARGETS_INSTALL_FILE));
      if (!file.open(QIODevice::ReadOnly))
        return QJsonArray{};
    }
    return QJsonDocument::fromJson(file.readAll()).array();
  }();
  return targets;
}
QJsonObject extensionTarget(const QString &id) {
  for (const auto &value : extensionTargets())
    if (value.toObject().value("id").toString() == id)
      return value.toObject();
  return {};
}
QJsonObject extensionDefaults(const QJsonObject &schema) {
  return Settings::defaults(schema);
}
bool validateExtensionSettings(const QJsonObject &schema,
                               const QJsonObject &settings, QString *error) {
  return Settings::validateSchema(schema, error) &&
         Settings::validateValues(schema, settings, error);
}
} // namespace LunaDash
