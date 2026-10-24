#pragma once

#include "core/json/JsonAccess.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <regex>
#include <string>
#include <string_view>

namespace LunaDash::Settings {
// Shared data contract. Rendering choices never weaken value validation.
namespace Detail {
inline bool arrayContains(const Json &array, const Json &value) {
  return array.is_array() &&
         std::find(array.begin(), array.end(), value) != array.end();
}

inline bool patternMatches(const std::string &pattern, const std::string &text,
                           bool *valid = nullptr) {
  try {
    const std::regex expression(pattern, std::regex::ECMAScript);
    if (valid)
      *valid = true;
    return std::regex_search(text, expression);
  } catch (const std::regex_error &) {
    if (valid)
      *valid = false;
    return false;
  }
}

inline bool validPattern(const std::string &pattern) {
  bool valid = false;
  patternMatches(pattern, {}, &valid);
  return valid;
}

inline bool validKey(std::string_view key) {
  if (key.empty() ||
      !((key.front() >= 'A' && key.front() <= 'Z') ||
        (key.front() >= 'a' && key.front() <= 'z')))
    return false;
  return std::all_of(key.begin() + 1, key.end(), [](char character) {
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9') || character == '_';
  });
}

constexpr double kMaximumSafeInteger = 9007199254740991.0;
} // namespace Detail

inline std::string control(const Json &rule) {
  if (jsonContains(rule, "control"))
    return jsonString(rule, "control");
  if (jsonContains(rule, "enum"))
    return "select";
  const auto type = jsonString(rule, "type");
  if (type == "boolean")
    return "toggle";
  if (type == "integer" || type == "number")
    return jsonContains(rule, "minimum") && jsonContains(rule, "maximum")
               ? "slider"
               : "number";
  return "text"; // Backward-compatible paths, colors and free-form strings.
}

inline bool validValue(const Json &rule, const Json &value) {
  const auto type = jsonString(rule, "type");
  bool valid = false;
  if (type == "boolean")
    valid = value.is_boolean();
  else if (type == "string") {
    valid = value.is_string() && value.get_ref<const std::string &>().size() <= 4096;
    if (valid && jsonContains(rule, "pattern"))
      valid = Detail::patternMatches(jsonString(rule, "pattern"),
                                     value.get<std::string>());
  } else if (type == "number" || type == "integer") {
    const double n = jsonAsDouble(value);
    const bool special =
        Detail::arrayContains(jsonValue(rule, "specialValues"), value);
    valid = value.is_number() && std::isfinite(n) &&
            std::abs(n) <= Detail::kMaximumSafeInteger &&
            (type != "integer" || n == std::floor(n)) &&
            (special || ((!jsonContains(rule, "minimum") ||
                          n >= jsonDouble(rule, "minimum")) &&
                         (!jsonContains(rule, "maximum") ||
                          n <= jsonDouble(rule, "maximum"))));
  } else if (type == "array") {
    const auto items = jsonObject(rule, "items");
    valid = value.is_array() &&
            value.size() <= static_cast<std::size_t>(
                                std::max(0, jsonInt(rule, "maxItems", 64)));
    if (valid) {
      Json seen = Json::array();
      for (const auto &entry : value) {
        if (!validValue(items, entry) || Detail::arrayContains(seen, entry)) {
          valid = false;
          break;
        }
        seen.push_back(entry);
      }
    }
  }
  return valid && (!jsonContains(rule, "enum") ||
                   Detail::arrayContains(jsonValue(rule, "enum"), value));
}

inline bool validateSchema(const Json &schema, std::string *error = nullptr) {
  const auto fail = [error](const std::string &key) {
    if (error)
      *error = "Invalid settings schema: " + key;
    return false;
  };
  if (!schema.is_object())
    return fail("schema is not an object");
  if (schema.size() > 128)
    return fail("too many fields");
  for (auto it = schema.begin(); it != schema.end(); ++it) {
    if (!Detail::validKey(it.key()) || !it.value().is_object())
      return fail(it.key());
    const auto &rule = it.value();
    const auto type = jsonString(rule, "type");
    const bool numeric = type == "integer" || type == "number";
    static constexpr std::array<std::string_view, 5> types{
        "boolean", "string", "integer", "number", "array"};
    if (std::find(types.begin(), types.end(), type) == types.end() ||
        !jsonContains(rule, "default"))
      return fail(it.key());
    for (const auto *key : {"label", "description", "group", "pattern", "control"}) {
      const auto *member = jsonMember(rule, key);
      if (member && (!member->is_string() ||
                     member->get_ref<const std::string &>().size() > 4096))
        return fail(it.key());
    }
    if (const auto *readOnly = jsonMember(rule, "readOnly");
        readOnly && !readOnly->is_boolean())
      return fail(it.key());
    for (const std::string_view key : {"minimum", "maximum", "step", "order"}) {
      const auto *value = jsonMember(rule, key);
      if (!value)
        continue;
      const double n = jsonAsDouble(*value);
      if (!value->is_number() || !std::isfinite(n) ||
          std::abs(n) > Detail::kMaximumSafeInteger ||
          (key != "order" && !numeric) ||
          (key == "step" &&
           (n <= 0 || (type == "integer" && n != std::floor(n)))))
        return fail(it.key());
    }
    if (jsonContains(rule, "minimum") && jsonContains(rule, "maximum") &&
        jsonDouble(rule, "minimum") > jsonDouble(rule, "maximum"))
      return fail(it.key());
    if (jsonContains(rule, "pattern") &&
        (type != "string" || !Detail::validPattern(jsonString(rule, "pattern"))))
      return fail(it.key());
    if (const auto *specials = jsonMember(rule, "specialValues")) {
      if (!numeric || !specials->is_array() || specials->size() > 16)
        return fail(it.key());
      for (const auto &special : *specials) {
        const double n = jsonAsDouble(special);
        if (!special.is_number() || !std::isfinite(n) ||
            std::abs(n) > Detail::kMaximumSafeInteger ||
            (type == "integer" && n != std::floor(n)))
          return fail(it.key());
      }
    }
    if (const auto *options = jsonMember(rule, "enum")) {
      if (!options->is_array() || options->empty() || options->size() > 128)
        return fail(it.key());
      auto withoutEnum = rule;
      withoutEnum.erase("enum");
      Json seen = Json::array();
      for (const auto &option : *options) {
        if (!validValue(withoutEnum, option) || Detail::arrayContains(seen, option))
          return fail(it.key());
        seen.push_back(option);
      }
    }
    if (type == "array") {
      const auto *items = jsonMember(rule, "items");
      const auto *maxItems = jsonMember(rule, "maxItems");
      if (!items || !items->is_object() || jsonString(*items, "type") != "string" ||
          !jsonValue(*items, "enum").is_array() ||
          jsonValue(*items, "enum").empty() ||
          (maxItems && (!maxItems->is_number() || !jsonIsIntegral(*maxItems) ||
                        jsonAsInt(*maxItems, -1) < 0 || jsonAsInt(*maxItems, -1) > 64)))
        return fail(it.key());
      auto itemRule = *items;
      itemRule["default"] = jsonValue(*items, "enum").front();
      if (!validateSchema(Json{{"item", itemRule}}, error))
        return false;
    }
    const auto widget = control(rule);
    const bool compatible =
        (widget == "toggle" && type == "boolean") ||
        (widget == "select" && (jsonContains(rule, "enum") || type == "array")) ||
        (widget == "number" && numeric) ||
        (widget == "slider" && numeric && jsonContains(rule, "minimum") &&
         jsonContains(rule, "maximum") && !jsonContains(rule, "specialValues")) ||
        (widget == "text" && type == "string");
    if (!compatible || !validValue(rule, jsonValue(rule, "default")))
      return fail(it.key());
  }
  return true;
}

inline bool validatePluginSchema(const Json &schema, std::string *error = nullptr) {
  if (!validateSchema(schema, error))
    return false;
  static constexpr std::array<std::string_view, 4> allowedControls{
      "toggle", "select", "number", "slider"};
  for (auto it = schema.begin(); it != schema.end(); ++it) {
    const auto widget = control(it.value());
    if (jsonString(it.value(), "type") == "array" ||
        std::find(allowedControls.begin(), allowedControls.end(), widget) ==
            allowedControls.end()) {
      if (error)
        *error =
            "Plugin settings support only toggle, select, number and slider: " +
            it.key();
      return false;
    }
  }
  return true;
}

inline Json defaults(const Json &schema) {
  Json values = Json::object();
  if (!schema.is_object())
    return values;
  for (auto it = schema.begin(); it != schema.end(); ++it)
    values[it.key()] = jsonValue(it.value(), "default");
  return values;
}

inline bool validateValues(const Json &schema, const Json &values,
                           std::string *error = nullptr) {
  if (!values.is_object()) {
    if (error)
      *error = "Settings values must be an object";
    return false;
  }
  for (auto it = values.begin(); it != values.end(); ++it) {
    const auto *rule = jsonMember(schema, it.key());
    if (!rule || !validValue(jsonAsObject(*rule), it.value())) {
      if (error)
        *error = "Invalid or unknown setting: " + it.key();
      return false;
    }
  }
  return true;
}

inline Json describe(const Json &schema) {
  Json result = Json::object();
  if (!schema.is_object())
    return result;
  for (auto it = schema.begin(); it != schema.end(); ++it) {
    auto rule = jsonAsObject(it.value());
    rule["control"] = control(rule);
    if (!jsonContains(rule, "label"))
      rule["label"] = it.key();
    result[it.key()] = rule;
  }
  return result;
}
} // namespace LunaDash::Settings
