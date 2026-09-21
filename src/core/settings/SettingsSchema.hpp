#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <algorithm>
#include <cmath>

namespace LunaDash::Settings {
// Shared data contract. Rendering choices never weaken value validation.
inline QString control(const QJsonObject &rule) {
  if (rule.contains("control"))
    return rule.value("control").toString();
  if (rule.contains("enum"))
    return "select";
  const auto type = rule.value("type").toString();
  if (type == "boolean")
    return "toggle";
  if (type == "integer" || type == "number")
    return rule.contains("minimum") && rule.contains("maximum") ? "slider"
                                                               : "number";
  return "text"; // Backward-compatible paths, colors and free-form strings.
}

inline bool validValue(const QJsonObject &rule, const QJsonValue &value) {
  const auto type = rule.value("type").toString();
  bool valid = false;
  if (type == "boolean")
    valid = value.isBool();
  else if (type == "string") {
    valid = value.isString() && value.toString().size() <= 4096;
    if (valid && rule.contains("pattern")) {
      const QRegularExpression pattern(rule.value("pattern").toString());
      valid = pattern.isValid() && pattern.match(value.toString()).hasMatch();
    }
  } else if (type == "number" || type == "integer") {
    const double n = value.toDouble();
    const bool special = rule.value("specialValues").toArray().contains(value);
    valid = value.isDouble() && std::isfinite(n) && std::abs(n) <= 9007199254740991.0 &&
            (type != "integer" || n == std::floor(n)) &&
            (special || ((!rule.contains("minimum") || n >= rule["minimum"].toDouble()) &&
                         (!rule.contains("maximum") || n <= rule["maximum"].toDouble())));
  } else if (type == "array") {
    const auto items = rule.value("items").toObject();
    const auto values = value.toArray();
    valid = value.isArray() && values.size() <= rule.value("maxItems").toInt(64);
    QJsonArray seen;
    for (const auto &entry : values) {
      if (!validValue(items, entry) || seen.contains(entry)) {
        valid = false;
        break;
      }
      seen.append(entry);
    }
  }
  return valid && (!rule.contains("enum") || rule["enum"].toArray().contains(value));
}

inline bool validateSchema(const QJsonObject &schema, QString *error = nullptr) {
  const auto fail = [error](const QString &key) {
    if (error)
      *error = "Invalid settings schema: " + key;
    return false;
  };
  if (schema.size() > 128)
    return fail("too many fields");
  static const QRegularExpression keyPattern("^[A-Za-z][A-Za-z0-9_]*$");
  for (auto it = schema.begin(); it != schema.end(); ++it) {
    if (!keyPattern.match(it.key()).hasMatch() || !it.value().isObject())
      return fail(it.key());
    const auto rule = it.value().toObject();
    const auto type = rule.value("type").toString();
    const bool numeric = type == "integer" || type == "number";
    if (!QStringList{"boolean", "string", "integer", "number", "array"}.contains(type) ||
        !rule.contains("default"))
      return fail(it.key());
    for (const auto &key : {"label", "description", "group", "pattern", "control"})
      if (rule.contains(key) && (!rule[key].isString() || rule[key].toString().size() > 4096))
        return fail(it.key());
    if (rule.contains("readOnly") && !rule["readOnly"].isBool())
      return fail(it.key());
    for (const auto &key : {"minimum", "maximum", "step", "order"}) {
      if (!rule.contains(key))
        continue;
      const auto value = rule[key];
      const double n = value.toDouble();
      if (!value.isDouble() || !std::isfinite(n) || std::abs(n) > 9007199254740991.0 ||
          (QString(key) != "order" && !numeric) ||
          (QString(key) == "step" && (n <= 0 || (type == "integer" && n != std::floor(n)))))
        return fail(it.key());
    }
    if (rule.contains("minimum") && rule.contains("maximum") &&
        rule["minimum"].toDouble() > rule["maximum"].toDouble())
      return fail(it.key());
    if (rule.contains("pattern") &&
        (type != "string" || !QRegularExpression(rule["pattern"].toString()).isValid()))
      return fail(it.key());
    if (rule.contains("specialValues")) {
      if (!numeric || !rule["specialValues"].isArray() || rule["specialValues"].toArray().size() > 16)
        return fail(it.key());
      for (const auto &special : rule["specialValues"].toArray())
        if (!special.isDouble() || !std::isfinite(special.toDouble()) ||
            std::abs(special.toDouble()) > 9007199254740991.0 ||
            (type == "integer" && special.toDouble() != std::floor(special.toDouble())))
          return fail(it.key());
    }
    if (rule.contains("enum")) {
      const auto options = rule["enum"].toArray();
      if (!rule["enum"].isArray() || options.isEmpty() || options.size() > 128)
        return fail(it.key());
      auto withoutEnum = rule;
      withoutEnum.remove("enum");
      QJsonArray seen;
      for (const auto &option : options) {
        if (!validValue(withoutEnum, option) || seen.contains(option))
          return fail(it.key());
        seen.append(option);
      }
    }
    if (type == "array") {
      const auto items = rule["items"].toObject();
      if (!rule["items"].isObject() || items["type"].toString() != "string" ||
          !items["enum"].isArray() || items["enum"].toArray().isEmpty() ||
          (rule.contains("maxItems") && (!rule["maxItems"].isDouble() ||
           rule["maxItems"].toDouble() != rule["maxItems"].toInt() ||
           rule["maxItems"].toInt() < 0 || rule["maxItems"].toInt() > 64)))
        return fail(it.key());
      auto itemRule = items;
      itemRule["default"] = items["enum"].toArray().first();
      if (!validateSchema({{"item", itemRule}}, error))
        return false;
    }
    const auto widget = control(rule);
    const bool compatible =
        (widget == "toggle" && type == "boolean") ||
        (widget == "select" && (rule.contains("enum") || type == "array")) ||
        (widget == "number" && numeric) ||
        (widget == "slider" && numeric && rule.contains("minimum") && rule.contains("maximum") &&
         !rule.contains("specialValues")) ||
        (widget == "text" && type == "string");
    if (!compatible || !validValue(rule, rule["default"]))
      return fail(it.key());
  }
  return true;
}

// QSettings INI storage returns numeric values as QString. Decode only when
// reading that legacy storage; IPC value validation remains strictly typed.
inline QJsonValue fromStoredValue(const QJsonObject &rule, const QVariant &stored) {
  auto value = QJsonValue::fromVariant(stored);
  const auto type = rule.value("type").toString();
  if (value.isString() && (type == "number" || type == "integer")) {
    bool ok = false;
    const auto number = value.toString().toDouble(&ok);
    if (ok) value = number;
  } else if (value.isString() && type == "boolean") {
    if (value.toString() == "true") value = true;
    else if (value.toString() == "false") value = false;
  }
  return value;
}

inline QJsonObject defaults(const QJsonObject &schema) {
  QJsonObject values;
  for (auto it = schema.begin(); it != schema.end(); ++it)
    values[it.key()] = it.value().toObject().value("default");
  return values;
}

inline bool validateValues(const QJsonObject &schema, const QJsonObject &values,
                           QString *error = nullptr) {
  for (auto it = values.begin(); it != values.end(); ++it) {
    if (!schema.contains(it.key()) || !validValue(schema[it.key()].toObject(), it.value())) {
      if (error)
        *error = "Invalid or unknown setting: " + it.key();
      return false;
    }
  }
  return true;
}

inline QJsonObject describe(const QJsonObject &schema) {
  QJsonObject result;
  for (auto it = schema.begin(); it != schema.end(); ++it) {
    auto rule = it.value().toObject();
    rule["control"] = control(rule);
    if (!rule.contains("label"))
      rule["label"] = it.key();
    result[it.key()] = rule;
  }
  return result;
}
} // namespace LunaDash::Settings
