#include "compositor/settings/SettingsApi.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/PluginCatalog.hpp"
#include "core/settings/SettingsTarget.hpp"
#include "shell/modules/ShellModules.hpp"
#include "shell/modules/ShellModuleSchema.hpp"
#include <QFile>
#include <QJsonDocument>

namespace LunaDash {
QJsonArray settingsApiTargets(const QJsonObject &extensions,
                              const QJsonObject &modules,
                              const QJsonObject &layout) {
  QJsonArray targets;
  for (const auto &entry : extensions.value("targets").toArray()) {
    const auto item = entry.toObject();
    // The layout template owns these settings; don't offer a second gap editor.
    if (item.value("id").toString() == "window-layout") continue;
    targets.append(Settings::target("builtin:" + item.value("id").toString(),
        item.value("name").toString(), "builtin", item.value("category").toString(),
        item.value("settings").toObject(), item.value("builtinSettings").toObject()));
  }
  for (const auto &entry : extensions.value("installed").toArray()) {
    const auto item = entry.toObject();
    if (!item.value("error").toString().isEmpty()) continue;
    targets.append(Settings::target("plugin:" + item.value("id").toString(),
        item.value("name").toString(), item.value("type").toString(),
        item.value("target").toString(), item.value("settingsSchema").toObject(),
        item.value("settings").toObject()));
  }
  const auto values = modules.value("document").toObject().value("modules").toObject();
  for (const auto &entry : modules.value("descriptors").toArray()) {
    const auto item = entry.toObject();
    const auto id = item.value("id").toString();
    const auto sections = item.value("sections").toObject();
    const auto module = values.value(id).toObject();
    for (auto it = sections.begin(); it != sections.end(); ++it) {
      const auto sectionValues = it.key() == "module"
          ? QJsonObject{{"enabled", module.value("enabled")}}
          : module.value(it.key()).toObject();
      targets.append(Settings::target("module:" + id + ":" + it.key(),
          item.value("name").toString() + " / " + it.key(), "module",
          item.value("type").toString(), it.value().toObject(), sectionValues));
    }
  }
  if (!layout.isEmpty()) targets.append(layout);
  return targets;
}

bool updateSettingsApi(PluginManager &plugins, ShellModules &modules,
                       const QJsonObject &layout, const QJsonObject &request,
                       const std::function<bool(const QJsonObject &, QString *)> &setLayout,
                       QString *error) {
  const auto fail = [error](const QString &message) {
    if (error) *error = message;
    return false;
  };
  for (auto it = request.begin(); it != request.end(); ++it)
    if (!QStringList{"target", "revision", "changes"}.contains(it.key()))
      return fail("Unknown settings API request field: " + it.key());
  const auto id = request.value("target").toString();
  if (id.isEmpty() || id.size() > 512) return fail("Invalid settings target.");
  plugins.refresh();
  auto moduleState = modules.snapshot();
  // Read current disk contents, not a potentially debounced watcher snapshot.
  if (id.startsWith("module:")) {
    QFile file(moduleState.value("path").toString());
    QJsonObject document;
    if (!file.open(QIODevice::ReadOnly) ||
        !validateModuleDocument(file.read(16385), &document, error))
      return fail("Module file changed or is unreadable; reload before editing.");
    moduleState["document"] = document;
  }
  const auto targets = settingsApiTargets(plugins.snapshot(), moduleState, layout);
  QJsonObject descriptor;
  for (const auto &entry : targets)
    if (entry.toObject().value("id").toString() == id) descriptor = entry.toObject();
  if (!Settings::checkPatch(descriptor, request, error)) return false;
  const auto changes = request.value("changes").toObject();
  if (id == layout.value("id").toString()) return setLayout(changes, error);
  const auto parts = id.split(':');
  if (parts.size() == 3 && parts[0] == "module") {
    auto document = moduleState.value("document").toObject();
    auto entries = document.value("modules").toObject();
    auto module = entries.value(parts[1]).toObject();
    if (parts[2] == "module") module = Settings::merged(module, changes);
    else module[parts[2]] = Settings::merged(module.value(parts[2]).toObject(), changes);
    entries[parts[1]] = module;
    document["modules"] = entries;
    return modules.apply(QJsonDocument(document).toJson(), error);
  }
  QString configError;
  auto document = readExtensionConfiguration(&configError);
  if (!configError.isEmpty()) return fail(configError);
  if (parts.size() != 2) return fail("Invalid settings target.");
  if (parts[0] == "builtin") {
    auto builtins = document.value("builtins").toObject();
    builtins[parts[1]] = Settings::merged(builtins.value(parts[1]).toObject(), changes);
    document["builtins"] = builtins;
  } else if (parts[0] == "plugin") {
    bool found = false;
    for (const auto &plugin : discoverPlugins()) {
      if (plugin.id != parts[1]) continue;
      if (!plugin.error.isEmpty()) return fail(plugin.error);
      auto entries = document.value("plugins").toObject();
      entries[plugin.id] = QJsonObject{{"enabled", plugin.enabled}, {"mode", plugin.mode},
          {"settings", Settings::merged(plugin.settings, changes)}};
      document["plugins"] = entries;
      found = true;
      break;
    }
    if (!found) return fail("Plugin is no longer installed.");
  } else return fail("Unknown settings target.");
  if (!saveExtensionConfiguration(QJsonDocument(document).toJson(), error)) return false;
  plugins.refresh();
  return true;
}
} // namespace LunaDash
