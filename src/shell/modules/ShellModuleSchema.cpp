#include "shell/modules/ShellModuleSchema.hpp"

#include <gio/gio.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace LunaDash {
namespace {

bool fail(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
  return false;
}

bool jsonNumber(const ShellModuleJson &value) {
  return value.is_number();
}

double numberValue(const ShellModuleJson &value) {
  return value.get<double>();
}

bool validPattern(std::string_view pattern) {
  GError *error = nullptr;
  auto *regex = g_regex_new(std::string(pattern).c_str(), G_REGEX_OPTIMIZE,
                            G_REGEX_MATCH_DEFAULT, &error);
  if (error)
    g_error_free(error);
  if (!regex)
    return false;
  g_regex_unref(regex);
  return true;
}

bool patternMatches(std::string_view pattern, std::string_view value) {
  GError *error = nullptr;
  auto *regex = g_regex_new(std::string(pattern).c_str(), G_REGEX_OPTIMIZE,
                            G_REGEX_MATCH_DEFAULT, &error);
  if (error)
    g_error_free(error);
  if (!regex)
    return false;
  const bool matched = g_regex_match(regex, std::string(value).c_str(),
                                     G_REGEX_MATCH_DEFAULT, nullptr);
  g_regex_unref(regex);
  return matched;
}

bool validValue(const ShellModuleJson &rule, const ShellModuleJson &value);

bool validValue(const ShellModuleJson &rule, const ShellModuleJson &value) {
  const auto type = rule.value("type", std::string{});
  bool valid = false;
  if (type == "boolean") {
    valid = value.is_boolean();
  } else if (type == "string") {
    valid = value.is_string() && value.get_ref<const std::string &>().size() <= 4096;
    if (valid && rule.contains("pattern"))
      valid = patternMatches(rule.at("pattern").get<std::string>(),
                             value.get_ref<const std::string &>());
  } else if (type == "number" || type == "integer") {
    if (jsonNumber(value)) {
      const double number = numberValue(value);
      const auto specialValues =
          rule.value("specialValues", ShellModuleJson::array());
      const bool special = specialValues.is_array() &&
                           std::find(specialValues.begin(), specialValues.end(),
                                     value) != specialValues.end();
      valid = std::isfinite(number) &&
              std::abs(number) <= 9007199254740991.0 &&
              (type != "integer" || std::floor(number) == number) &&
              (special ||
               ((!rule.contains("minimum") ||
                 number >= numberValue(rule.at("minimum"))) &&
                (!rule.contains("maximum") ||
                 number <= numberValue(rule.at("maximum")))));
    }
  } else if (type == "array") {
    const auto items = rule.value("items", ShellModuleJson::object());
    const auto maximum = rule.value("maxItems", 64);
    valid = value.is_array() && maximum >= 0 &&
            value.size() <= static_cast<std::size_t>(maximum);
    std::set<ShellModuleJson> seen;
    if (valid) {
      for (const auto &entry : value) {
        if (!validValue(items, entry) || !seen.insert(entry).second) {
          valid = false;
          break;
        }
      }
    }
  }
  if (!valid)
    return false;
  if (rule.contains("enum")) {
    const auto &options = rule.at("enum");
    return options.is_array() &&
           std::find(options.begin(), options.end(), value) != options.end();
  }
  return true;
}

std::string control(const ShellModuleJson &rule) {
  if (rule.contains("control"))
    return rule.value("control", std::string{});
  if (rule.contains("enum"))
    return "select";
  const auto type = rule.value("type", std::string{});
  if (type == "boolean")
    return "toggle";
  if (type == "integer" || type == "number")
    return rule.contains("minimum") && rule.contains("maximum") ? "slider"
                                                                 : "number";
  return "text";
}

bool validateSchema(const ShellModuleJson &schema, std::string *error);

bool validateSchema(const ShellModuleJson &schema, std::string *error) {
  if (!schema.is_object() || schema.size() > 128)
    return fail(error, "Invalid settings schema: too many fields");
  for (auto it = schema.begin(); it != schema.end(); ++it) {
    const auto &key = it.key();
    const auto &rule = it.value();
    const auto type = rule.is_object() ? rule.value("type", std::string{}) : "";
    const bool numeric = type == "integer" || type == "number";
    if (key.empty() || !std::isalpha(static_cast<unsigned char>(key.front())) ||
        !std::all_of(key.begin() + 1, key.end(), [](unsigned char c) {
          return std::isalnum(c) || c == '_';
        }) ||
        !rule.is_object() ||
        (type != "boolean" && type != "string" && !numeric && type != "array") ||
        !rule.contains("default"))
      return fail(error, "Invalid settings schema: " + key);

    for (const auto *name : {"label", "description", "group", "pattern", "control"}) {
      if (rule.contains(name) &&
          (!rule.at(name).is_string() || rule.at(name).get<std::string>().size() > 4096))
        return fail(error, "Invalid settings schema: " + key);
    }
    if (rule.contains("readOnly") && !rule.at("readOnly").is_boolean())
      return fail(error, "Invalid settings schema: " + key);
    for (const auto *name : {"minimum", "maximum", "step", "order"}) {
      if (!rule.contains(name))
        continue;
      const auto &value = rule.at(name);
      if (!jsonNumber(value))
        return fail(error, "Invalid settings schema: " + key);
      const double number = numberValue(value);
      if (!std::isfinite(number) || std::abs(number) > 9007199254740991.0 ||
          (std::string_view(name) != "order" && !numeric) ||
          (std::string_view(name) == "step" &&
           (number <= 0 || (type == "integer" && std::floor(number) != number))))
        return fail(error, "Invalid settings schema: " + key);
    }
    if (rule.contains("minimum") && rule.contains("maximum") &&
        numberValue(rule.at("minimum")) > numberValue(rule.at("maximum")))
      return fail(error, "Invalid settings schema: " + key);
    if (rule.contains("pattern") &&
        (type != "string" || !validPattern(rule.at("pattern").get<std::string>())))
      return fail(error, "Invalid settings schema: " + key);
    if (rule.contains("specialValues")) {
      const auto &specials = rule.at("specialValues");
      if (!numeric || !specials.is_array() || specials.size() > 16)
        return fail(error, "Invalid settings schema: " + key);
      for (const auto &special : specials) {
        if (!jsonNumber(special) || !std::isfinite(numberValue(special)) ||
            std::abs(numberValue(special)) > 9007199254740991.0 ||
            (type == "integer" && std::floor(numberValue(special)) != numberValue(special)))
          return fail(error, "Invalid settings schema: " + key);
      }
    }
    if (rule.contains("enum")) {
      const auto &options = rule.at("enum");
      if (!options.is_array() || options.empty() || options.size() > 128)
        return fail(error, "Invalid settings schema: " + key);
      auto withoutEnum = rule;
      withoutEnum.erase("enum");
      std::set<ShellModuleJson> seen;
      for (const auto &option : options) {
        if (!validValue(withoutEnum, option) || !seen.insert(option).second)
          return fail(error, "Invalid settings schema: " + key);
      }
    }
    if (type == "array") {
      const auto items = rule.value("items", ShellModuleJson::object());
      const auto maximum = rule.value("maxItems", 64);
      if (!items.is_object() || items.value("type", std::string{}) != "string" ||
          !items.value("enum", ShellModuleJson::array()).is_array() ||
          items.value("enum", ShellModuleJson::array()).empty() || maximum < 0 || maximum > 64)
        return fail(error, "Invalid settings schema: " + key);
      auto itemRule = items;
      itemRule["default"] = items.at("enum").front();
      if (!validateSchema({{"item", itemRule}}, error))
        return false;
    }
    const auto widget = control(rule);
    const bool compatible =
        (widget == "toggle" && type == "boolean") ||
        (widget == "select" && (rule.contains("enum") || type == "array")) ||
        (widget == "number" && numeric) ||
        (widget == "slider" && numeric && rule.contains("minimum") &&
         rule.contains("maximum") && !rule.contains("specialValues")) ||
        (widget == "text" && type == "string");
    if (!compatible || !validValue(rule, rule.at("default")))
      return fail(error, "Invalid settings schema: " + key);
  }
  return true;
}

std::filesystem::path registryPath() {
  const std::filesystem::path source(LUDASH_MODULE_REGISTRY_SOURCE);
  if (std::filesystem::exists(source))
    return source;
  return std::filesystem::path(LUDASH_MODULE_REGISTRY_INSTALL);
}

ShellModuleJson loadRegistry() {
  std::ifstream file(registryPath(), std::ios::binary);
  if (!file)
    throw std::runtime_error("Missing module settings registry");
  auto registry = ShellModuleJson::parse(file, nullptr, false);
  if (registry.is_discarded() || !registry.is_object() ||
      registry.value("schemaVersion", 0) != 1 ||
      !registry.value("modules", ShellModuleJson::array()).is_array())
    throw std::runtime_error("Invalid module settings registry");

  const auto common = registry.value("common", ShellModuleJson::object());
  auto modules = registry.at("modules");
  std::set<std::string> ids;
  for (auto &descriptor : modules) {
    if (!descriptor.is_object())
      throw std::runtime_error("Invalid module settings descriptor");
    const auto id = descriptor.value("id", std::string{});
    if (id.empty() || !ids.insert(id).second)
      throw std::runtime_error("Duplicate or empty module identity in registry");
    auto sections = descriptor.value("sections", ShellModuleJson::object());
    for (auto section = common.begin(); section != common.end(); ++section) {
      auto fields = section.value();
      const auto overrides = sections.value(section.key(), ShellModuleJson::object());
      if (!fields.is_object() || !overrides.is_object())
        throw std::runtime_error("Invalid common module settings");
      for (auto field = overrides.begin(); field != overrides.end(); ++field)
        fields[field.key()] = field.value();
      sections[section.key()] = std::move(fields);
    }
    if (!sections.is_object())
      throw std::runtime_error("Invalid module settings sections");
    for (auto section = sections.begin(); section != sections.end(); ++section) {
      std::string error;
      if (!validateSchema(section.value(), &error))
        throw std::runtime_error("Invalid module settings registry: " + error);
    }
    descriptor["sections"] = std::move(sections);
  }
  if (modules.empty())
    throw std::runtime_error("Module settings registry is empty");
  registry["modules"] = std::move(modules);
  return registry;
}

} // namespace

const ShellModuleJson &shellModuleDescriptors() {
  static const auto descriptors = loadRegistry().at("modules");
  return descriptors;
}

ShellModuleJson shellModuleDescriptor(std::string_view id) {
  for (const auto &entry : shellModuleDescriptors())
    if (entry.value("id", std::string{}) == id)
      return entry;
  return ShellModuleJson::object();
}

std::vector<std::string> shellModuleIds() {
  std::vector<std::string> ids;
  for (const auto &entry : shellModuleDescriptors())
    ids.push_back(entry.value("id", std::string{}));
  return ids;
}

ShellModuleJson defaultModuleDocument() {
  ShellModuleJson modules = ShellModuleJson::object();
  for (const auto &descriptor : shellModuleDescriptors()) {
    const auto sections = descriptor.value("sections", ShellModuleJson::object());
    ShellModuleJson module = ShellModuleJson::object();
    for (const auto *section : {"module", "style", "config"}) {
      ShellModuleJson values = ShellModuleJson::object();
      const auto schema = sections.value(section, ShellModuleJson::object());
      for (auto field = schema.begin(); field != schema.end(); ++field)
        values[field.key()] = field.value().at("default");
      if (std::string_view(section) == "module") {
        for (auto value = values.begin(); value != values.end(); ++value)
          module[value.key()] = value.value();
      } else {
        module[section] = std::move(values);
      }
    }
    modules[descriptor.at("id").get<std::string>()] = std::move(module);
  }
  return {{"schemaVersion", 1}, {"modules", std::move(modules)}};
}

bool validateModuleDocument(std::string_view text, ShellModuleJson *normalized,
                            std::string *error) {
  if (!normalized || text.size() > 16384)
    return fail(error, "Module JSON must be at most 16 KiB.");
  auto root = ShellModuleJson::parse(text.begin(), text.end(), nullptr, false);
  if (root.is_discarded() || !root.is_object())
    return fail(error, "Invalid module JSON.");
  for (auto it = root.begin(); it != root.end(); ++it)
    if (it.key() != "schemaVersion" && it.key() != "modules")
      return fail(error, "Expected schemaVersion 1 and a modules object.");
  if (!root.contains("schemaVersion") || root.at("schemaVersion") != 1 ||
      !root.value("modules", ShellModuleJson{}).is_object())
    return fail(error, "Expected schemaVersion 1 and a modules object.");

  auto result = defaultModuleDocument();
  auto modules = result.at("modules");
  const auto &input = root.at("modules");
  for (auto it = input.begin(); it != input.end(); ++it) {
    if (!modules.contains(it.key()) || !it.value().is_object())
      return fail(error, "Unknown module or invalid object: " + it.key());
    const auto &source = it.value();
    for (auto field = source.begin(); field != source.end(); ++field)
      if (field.key() != "enabled" && field.key() != "style" &&
          field.key() != "config" && field.key() != "custom")
        return fail(error, "Unknown module field: " + it.key());
    auto module = modules.at(it.key());
    const auto sections = shellModuleDescriptor(it.key()).at("sections");
    if (source.contains("enabled")) {
      const auto schema = sections.value("module", ShellModuleJson::object());
      if (!schema.contains("enabled") || !validValue(schema.at("enabled"), source.at("enabled")))
        return fail(error, "Invalid or unknown setting: enabled");
      module["enabled"] = source.at("enabled");
    }
    for (const auto *section : {"style", "config"}) {
      if (!source.contains(section))
        continue;
      if (!source.at(section).is_object())
        return fail(error, "Module section must be an object: " + std::string(section));
      const auto schema = sections.value(section, ShellModuleJson::object());
      const auto &changes = source.at(section);
      for (auto field = changes.begin(); field != changes.end(); ++field) {
        if (!schema.contains(field.key()) || !validValue(schema.at(field.key()), field.value()))
          return fail(error, "Invalid or unknown setting: " + field.key());
        module.at(section)[field.key()] = field.value();
      }
    }
    const auto config = module.value("config", ShellModuleJson::object());
    if (config.contains("workspaceActiveWidth") &&
        config.at("workspaceActiveWidth").get<int>() <
            config.at("workspaceInactiveWidth").get<int>())
      return fail(error, "Active workspace pill must be at least as wide as an inactive pill.");
    // The legacy custom field is accepted for migration but is never loaded.
    modules[it.key()] = std::move(module);
  }
  result["modules"] = std::move(modules);
  *normalized = std::move(result);
  return true;
}

} // namespace LunaDash
