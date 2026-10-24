#pragma once
#include "core/crypto/Sha256.hpp"
#include "core/settings/SettingsSchema.hpp"

namespace LunaDash::Settings {
inline std::string revision(const Json &schema, const Json &values) {
  return sha256Hex(jsonCompact(Json{{"schema", schema}, {"values", values}}));
}
inline Json target(const std::string &id, const std::string &name,
                   const std::string &type, const std::string &category,
                   const Json &schema, const Json &values) {
  const auto normalized = describe(schema);
  return {{"id", id}, {"name", name}, {"type", type}, {"category", category},
          {"schema", normalized}, {"values", values},
          {"revision", revision(normalized, values)}};
}
inline bool checkPatch(const Json &descriptor, const Json &request,
                       std::string *error) {
  const auto fail = [error](const std::string &message) {
    if (error) *error = message;
    return false;
  };
  if (!descriptor.is_object() || descriptor.empty() ||
      !jsonValue(request, "changes").is_object())
    return fail("Unknown settings target or invalid changes object.");
  if (!jsonValue(request, "revision").is_string() ||
      jsonValue(request, "revision") != jsonValue(descriptor, "revision"))
    return fail("Settings changed. Reload before applying this edit.");
  const auto schema = jsonObject(descriptor, "schema");
  const auto changes = jsonObject(request, "changes");
  if (!validateSchema(schema, error) || !validateValues(schema, changes, error))
    return false;
  const auto values = jsonObject(descriptor, "values");
  for (auto it = changes.begin(); it != changes.end(); ++it)
    if (jsonBool(jsonObject(schema, it.key()), "readOnly") &&
        it.value() != jsonValue(values, it.key()))
      return fail("Setting is read-only: " + it.key());
  return true;
}
inline Json merged(Json values, const Json &changes) {
  if (!values.is_object())
    values = Json::object();
  if (changes.is_object())
    for (auto it = changes.begin(); it != changes.end(); ++it)
      values[it.key()] = it.value();
  return values;
}
} // namespace LunaDash::Settings
