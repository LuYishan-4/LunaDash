#pragma once
#include "core/settings/SettingsSchema.hpp"
#include <QCryptographicHash>

namespace LunaDash::Settings {
inline QString revision(const QJsonObject &schema, const QJsonObject &values) {
  return QString::fromLatin1(QCryptographicHash::hash(
      QJsonDocument(QJsonObject{{"schema", schema}, {"values", values}})
          .toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
inline QJsonObject target(const QString &id, const QString &name,
                          const QString &type, const QString &category,
                          const QJsonObject &schema, const QJsonObject &values) {
  const auto normalized = describe(schema);
  return {{"id", id}, {"name", name}, {"type", type}, {"category", category},
          {"schema", normalized}, {"values", values},
          {"revision", revision(normalized, values)}};
}
inline bool checkPatch(const QJsonObject &descriptor, const QJsonObject &request,
                       QString *error) {
  const auto fail = [error](const QString &message) {
    if (error) *error = message;
    return false;
  };
  if (descriptor.isEmpty() || !request.value("changes").isObject())
    return fail("Unknown settings target or invalid changes object.");
  if (!request.value("revision").isString() ||
      request.value("revision") != descriptor.value("revision"))
    return fail("Settings changed. Reload before applying this edit.");
  const auto schema = descriptor.value("schema").toObject();
  const auto changes = request.value("changes").toObject();
  if (!validateSchema(schema, error) || !validateValues(schema, changes, error))
    return false;
  for (auto it = changes.begin(); it != changes.end(); ++it)
    if (schema.value(it.key()).toObject().value("readOnly").toBool() &&
        it.value() != descriptor.value("values").toObject().value(it.key()))
      return fail("Setting is read-only: " + it.key());
  return true;
}
inline QJsonObject merged(QJsonObject values, const QJsonObject &changes) {
  for (auto it = changes.begin(); it != changes.end(); ++it)
    values[it.key()] = it.value();
  return values;
}
} // namespace LunaDash::Settings
