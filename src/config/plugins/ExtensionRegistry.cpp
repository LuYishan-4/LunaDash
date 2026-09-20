#include "config/plugins/ExtensionRegistry.hpp"
#include <QFile>
#include <QJsonDocument>
#include <cmath>

int qInitResources_extension_targets();
namespace LunaDash {
QJsonArray extensionTargets() {
  static const auto targets = [] {
    ::qInitResources_extension_targets();
    QFile file(":/LunaDash/plugins/targets.json");
    if (!file.open(QIODevice::ReadOnly))
      return QJsonArray{};
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
  QJsonObject result;
  for (auto it = schema.begin(); it != schema.end(); ++it)
    result[it.key()] = it.value().toObject().value("default");
  return result;
}
bool validateExtensionSettings(const QJsonObject &schema,
                               const QJsonObject &settings, QString *error) {
  for (auto it = settings.begin(); it != settings.end(); ++it) {
    const auto rule = schema.value(it.key()).toObject();
    const auto type = rule.value("type").toString();
    const auto value = it.value();
    bool valid = false;
    if (type == "boolean")
      valid = value.isBool();
    else if (type == "string")
      valid = value.isString() && value.toString().size() <= 4096;
    else if (type == "number" || type == "integer") {
      const double number = value.toDouble();
      valid =
          value.isDouble() && std::isfinite(number) &&
          (type != "integer" || number == std::floor(number)) &&
          (!rule.contains("minimum") || number >= rule["minimum"].toDouble()) &&
          (!rule.contains("maximum") || number <= rule["maximum"].toDouble());
    }
    if (valid && rule.contains("enum"))
      valid = rule.value("enum").toArray().contains(value);
    if (!valid) {
      if (error)
        *error = "Invalid or unknown extension setting: " + it.key();
      return false;
    }
  }
  return true;
}
} // namespace LunaDash
