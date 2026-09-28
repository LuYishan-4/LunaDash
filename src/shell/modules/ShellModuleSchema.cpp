#include "shell/modules/ShellModuleSchema.hpp"
#include "core/settings/SettingsSchema.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

int qInitResources_module_templates();
namespace LunaDash {
namespace {
bool fail(QString *error, const QString &text) {
  if (error)
    *error = text;
  return false;
}
bool keysAllowed(const QJsonObject &object, const QStringList &keys) {
  for (auto it = object.begin(); it != object.end(); ++it)
    if (!keys.contains(it.key()))
      return false;
  return true;
}
}
QJsonArray shellModuleDescriptors() {
  static const auto descriptors = [] {
    ::qInitResources_module_templates();
    QFile file(":/LuDash/data/modules/registry.json");
    if (!file.open(QIODevice::ReadOnly))
      qFatal("Missing embedded module settings registry");
    const auto document = QJsonDocument::fromJson(file.readAll());
    const auto modules = document.object().value("modules").toArray();
    const auto common = document.object().value("common").toObject();
    QJsonArray resolved;
    QSet<QString> ids;
    if (!document.isObject() || modules.isEmpty())
      qFatal("Invalid embedded module settings registry");
    for (const auto &entry : modules) {
      auto descriptor = entry.toObject();
      const auto id = descriptor.value("id").toString();
      if (id.isEmpty() || ids.contains(id))
        qFatal("Duplicate/empty module identity in embedded registry");
      ids.insert(id);
      auto sections = descriptor.value("sections").toObject();
      for (auto section = common.begin(); section != common.end(); ++section) {
        auto fields = section.value().toObject();
        const auto overrides = sections.value(section.key()).toObject();
        for (auto field = overrides.begin(); field != overrides.end(); ++field)
          fields[field.key()] = field.value();
        sections[section.key()] = fields;
      }
      descriptor["sections"] = sections;
      for (auto it = sections.begin(); it != sections.end(); ++it) {
        QString error;
        if (!Settings::validateSchema(it.value().toObject(), &error))
          qFatal("Invalid embedded module settings: %s", qPrintable(error));
      }
      resolved.append(descriptor);
    }
    return resolved;
  }();
  return descriptors;
}
QJsonObject shellModuleDescriptor(const QString &id) {
  for (const auto &entry : shellModuleDescriptors())
    if (entry.toObject().value("id").toString() == id)
      return entry.toObject();
  return {};
}
QStringList shellModuleIds() {
  QStringList ids;
  for (const auto &entry : shellModuleDescriptors())
    ids.append(entry.toObject().value("id").toString());
  return ids;
}
QJsonObject defaultModuleDocument() {
  QJsonObject modules;
  for (const auto &entry : shellModuleDescriptors()) {
    const auto descriptor = entry.toObject();
    const auto sections = descriptor.value("sections").toObject();
    auto module = Settings::defaults(sections.value("module").toObject());
    for (const auto &section : {"style", "config"})
      module[section] = Settings::defaults(sections.value(section).toObject());
    modules[descriptor.value("id").toString()] = module;
  }
  return {{"schemaVersion", 1}, {"modules", modules}};
}
bool validateModuleDocument(const QByteArray &text, QJsonObject *normalized,
                            QString *error) {
  if (!normalized || text.size() > 16384)
    return fail(error, "Module JSON must be at most 16 KiB.");
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(text, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
    return fail(error, "Invalid module JSON: " + parseError.errorString());
  const auto root = document.object();
  if (!keysAllowed(root, {"schemaVersion", "modules"}) ||
      root.value("schemaVersion") != QJsonValue(1) || !root.value("modules").isObject())
    return fail(error, "Expected schemaVersion 1 and a modules object.");
  auto result = defaultModuleDocument();
  auto modules = result.value("modules").toObject();
  const auto input = root.value("modules").toObject();
  for (auto it = input.begin(); it != input.end(); ++it) {
    if (!modules.contains(it.key()) || !it.value().isObject())
      return fail(error, "Unknown module or invalid object: " + it.key());
    const auto source = it.value().toObject();
    if (!keysAllowed(source, {"enabled", "style", "config", "custom"}))
      return fail(error, "Unknown module field: " + it.key());
    auto module = modules.value(it.key()).toObject();
    const auto sections = shellModuleDescriptor(it.key()).value("sections").toObject();
    if (source.contains("enabled")) {
      if (!Settings::validateValues(sections.value("module").toObject(),
                                    {{"enabled", source.value("enabled")}}, error))
        return false;
      module["enabled"] = source.value("enabled");
    }
    for (const auto &section : {"style", "config"}) {
      if (!source.contains(section))
        continue;
      if (!source.value(section).isObject())
        return fail(error, "Module section must be an object: " + QString(section));
      const auto changes = source.value(section).toObject();
      if (!Settings::validateValues(sections.value(section).toObject(), changes, error))
        return false;
      auto values = module.value(section).toObject();
      for (auto field = changes.begin(); field != changes.end(); ++field)
        values[field.key()] = field.value();
      module[section] = values;
    }
    // Cross-field and lifecycle invariants are host-owned, not UI overrides.
    const auto config = module.value("config").toObject();
    if (config.contains("workspaceActiveWidth") &&
        config.value("workspaceActiveWidth").toInt() < config.value("workspaceInactiveWidth").toInt())
      return fail(error, "Active workspace pill must be at least as wide as an inactive pill.");
    // Accept the old field only for migration. It is not exposed in the
    // schema or copied into the normalized document and can never be loaded.
    modules[it.key()] = module;
  }
  result["modules"] = modules;
  *normalized = result;
  return true;
}
} // namespace LunaDash
