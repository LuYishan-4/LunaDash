#include "compositor/plugins/PluginManager.hpp"
#include "compositor/plugins/PluginBundle.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "core/plugins/PluginApi.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLibrary>
#include <QScopedValueRollback>
#include <QSet>
#include <QUrl>
#include <algorithm>

namespace LunaDash {
struct PluginManager::Native {
  std::shared_ptr<PluginBundle> bundle;
  QLibrary library;
  QString revision;
  ~Native() { library.unload(); }
  PluginDescriptor descriptor;
  PluginDescriptor current;
  const ludash_plugin_api *api = nullptr;
};
PluginManager::PluginManager(QObject *parent) : QObject(parent) {
  poll_.setInterval(1000);
  connect(&poll_, &QTimer::timeout, this, &PluginManager::refresh);
  poll_.start();
}
PluginManager::~PluginManager() = default;
void PluginManager::loadEnabled() { refresh(); }
void PluginManager::refresh() {
  if (inHook_)
    return;
  const auto discovered = discoverPlugins();
  QList<PluginDescriptor> catalog;
  QSet<QString> active;
  bool changedCode = false;
  for (auto descriptor : discovered) {
    const auto id = descriptor.id;
    if (!descriptor.enabled || !descriptor.error.isEmpty()) {
      catalog.append(descriptor);
      continue;
    }
    QString error;
    const auto directory = QFileInfo(descriptor.metadataPath).absolutePath();
    const auto revision = PluginBundle::fingerprint(directory, &error);
    if (revision.isEmpty()) {
      descriptor.error = error;
      catalog.append(descriptor);
      continue;
    }
    const auto attempt =
        revision + descriptor.mode +
        QString::fromUtf8(
            QJsonDocument(descriptor.settings).toJson(QJsonDocument::Compact));
    if (attempts_.value(id) != attempt) {
      errors_.remove(id);
      attempts_[id] = attempt;
      changedCode = true;
    }
    if (revisions_.value(id) != revision || !bundles_.contains(id)) {
      const auto bundle = PluginBundle::copy(directory, &error);
      if (!bundle || revision != PluginBundle::fingerprint(directory, &error)) {
        descriptor.error = error.isEmpty() ? "Plugin changed during reload; "
                                             "waiting for a complete revision"
                                           : error;
        catalog.append(descriptor);
        continue;
      }
      // Verify the private copy against the manifest and SDK receipt too.
      const auto copied = readPluginMetadata(bundle->file("metadata.json"));
      if (!copied.error.isEmpty() || copied.manifest != descriptor.manifest) {
        descriptor.error =
            "Plugin changed during reload; waiting for a valid SDK build";
        catalog.append(descriptor);
        continue;
      }
      if (bundles_.contains(id))
        retired_.append(bundles_[id]);
      bundles_[id] = bundle;
      revisions_[id] = revision;
      // Keep recent QML revisions alive while the shell replaces asynchronous
      // image/component loads. Native revisions have their own shared owner.
      while (retired_.size() > 16)
        retired_.removeFirst();
    }
    const auto bundle = bundles_.value(id);
    if (descriptor.type == "quickshell")
      descriptor.entryPath =
          bundle->file(QFileInfo(descriptor.entryPath).fileName());
    else if (descriptor.type == "opengl") {
      for (const auto &stage : {QString("vertex"), QString("fragment")})
        descriptor.shaders[stage] =
            QUrl::fromLocalFile(
                bundle->file(descriptor.manifest.value("shaders")
                                 .toObject()
                                 .value(stage)
                                 .toString() +
                             ".qsb"))
                .toString();
    } else if (descriptor.type == "effect") {
      active.insert(id);
      auto found =
          std::find_if(native_.begin(), native_.end(), [&](const auto &entry) {
            return entry->descriptor.id == id;
          });
      if (found != native_.end() && (*found)->revision != revision) {
        native_.erase(found);
        found = native_.end();
      }
      if (found != native_.end()) {
        (*found)->current = descriptor;
      } else if (!errors_.contains(id)) {
        auto native = std::make_unique<Native>();
        native->descriptor = native->current = descriptor;
        native->bundle = bundle;
        native->revision = revision;
        native->library.setFileName(
            bundle->file(QFileInfo(descriptor.libraryPath).fileName()));
        using Entry = const ludash_plugin_api *(*)();
        const auto entry = reinterpret_cast<Entry>(
            native->library.resolve("ludash_plugin_entry_v2"));
        if (!entry) {
          errors_[id] = native->library.errorString();
        } else {
          native->api = entry();
          const auto *api = native->api;
          if (!api || api->struct_size != sizeof(ludash_plugin_api) ||
              api->api_version != LUDASH_PLUGIN_API_VERSION || !api->process ||
              !api->metadata_json ||
              QJsonDocument::fromJson(api->metadata_json).object() !=
                  descriptor.manifest)
            errors_[id] = "SDK ABI or embedded metadata mismatch";
          else
            native_.push_back(std::move(native));
        }
      }
    }
    catalog.append(descriptor);
  }
  const auto removed = std::erase_if(native_, [&](const auto &entry) {
    return !active.contains(entry->descriptor.id);
  });
  for (const auto &id : bundles_.keys())
    if (!std::any_of(catalog.begin(), catalog.end(), [&](const auto &plugin) {
          return plugin.id == id && plugin.enabled;
        })) {
      retired_.append(bundles_.take(id));
      revisions_.remove(id);
      attempts_.remove(id);
    }
  while (retired_.size() > 16)
    retired_.removeFirst();
  std::sort(native_.begin(), native_.end(), [](const auto &a, const auto &b) {
    return a->descriptor.id < b->descriptor.id;
  });
  catalog_ = catalog;
  if (changedCode || removed)
    emit changed();
}
QJsonObject PluginManager::snapshot() {
  QJsonArray installed;
  QSet<QString> replacements;
  for (const auto &descriptor : catalog_) {
    auto item = pluginDescriptorJson(descriptor);
    QString error = descriptor.error;
    if (error.isEmpty())
      error = errors_.value(descriptor.id);
    if (descriptor.enabled && error.isEmpty() && descriptor.mode == "replace") {
      if (replacements.contains(descriptor.target))
        error = "Another replacement is already selected for this target";
      replacements.insert(descriptor.target);
    }
    bool loaded = descriptor.type != "effect";
    if (!loaded)
      loaded =
          std::any_of(native_.begin(), native_.end(), [&](const auto &entry) {
            return entry->descriptor.id == descriptor.id &&
                   entry->descriptor.manifest == descriptor.manifest;
          });
    item["error"] = error;
    item["available"] = descriptor.enabled && loaded && error.isEmpty();
    item["status"] = !error.isEmpty()      ? "error"
                     : !descriptor.enabled ? "disabled"
                     : !loaded             ? "unavailable"
                                           : "available";
    installed.append(item);
  }
  QString configError;
  const auto document = readExtensionConfiguration(&configError);
  auto targets = extensionTargets();
  for (int i = 0; i < targets.size(); ++i) {
    auto target = targets[i].toObject();
    target["builtinSettings"] =
        configuredBuiltinSettings(target.value("id").toString(), document);
    targets[i] = target;
  }
  return {{"installed", installed}, {"targets", targets},
          {"document", document},   {"path", extensionConfigurationPath()},
          {"error", configError},   {"storeSupported", false}};
}
bool PluginManager::setEnabled(const QString &id, bool enabled,
                               QString *error) {
  auto document = readExtensionConfiguration();
  auto configs = document.value("plugins").toObject();
  for (const auto &plugin : discoverPlugins()) {
    if (plugin.id != id)
      continue;
    configs[id] = QJsonObject{{"enabled", enabled},
                              {"mode", plugin.mode},
                              {"settings", plugin.settings}};
    document["plugins"] = configs;
    return saveExtensionConfiguration(QJsonDocument(document).toJson(), error);
  }
  if (error)
    *error = "Unknown plugin";
  return false;
}
void PluginManager::reportError(const QString &id, const QString &error) {
  if (!error.isEmpty())
    errors_[id] = error.left(1024);
  else {
    errors_.remove(id);
    attempts_.remove(id);
  }
}
bool PluginManager::stackingLayout() const {
  for (const auto &native : native_) {
    const auto &plugin = native->current;
    if (plugin.enabled && plugin.error.isEmpty() &&
        !errors_.contains(plugin.id) &&
        plugin.manifest == native->descriptor.manifest &&
        plugin.target == "window-layout" && plugin.mode == "replace")
      return plugin.manifest.value("layoutMode").toString("tiling") ==
             "stacking";
  }
  return false;
}
QJsonObject PluginManager::filter(const QString &target,
                                  const QJsonObject &builtin,
                                  const QJsonObject &context,
                                  const Validator &validate) {
  QScopedValueRollback<bool> guard(inHook_, true);
  QJsonObject current = builtin;
  bool replaced = false;
  for (const auto &mode : {QString("replace"), QString("augment")}) {
    for (const auto &native : native_) {
      const auto &descriptor = native->current;
      if (descriptor.target != target || descriptor.mode != mode ||
          !descriptor.enabled || !descriptor.error.isEmpty() ||
          descriptor.manifest != native->descriptor.manifest ||
          errors_.contains(descriptor.id) || (mode == "replace" && replaced))
        continue;
      const QJsonObject request{
          {"target", target},
          {"mode", mode},
          {"context", context},
          {"builtin", builtin},
          {"current", mode == "replace" ? QJsonObject{} : current},
          {"settings", descriptor.settings}};
      QByteArray response(262145, '\0');
      int length = -1;
      try {
        length = native->api->process(
            QJsonDocument(request).toJson(QJsonDocument::Compact).constData(),
            response.data(), static_cast<size_t>(response.size()));
      } catch (...) {
        reportError(descriptor.id, "Plugin threw across its C ABI");
        continue;
      }
      if (length < 0 || length >= response.size()) {
        reportError(descriptor.id,
                    "Plugin returned an invalid response length");
        continue;
      }
      const auto result = QJsonDocument::fromJson(response.left(length));
      if (!result.isObject() || !validate(result.object())) {
        reportError(descriptor.id,
                    "Invalid plugin output; built-in/previous result retained");
        continue;
      }
      current = result.object();
      if (mode == "replace")
        replaced = true;
    }
  }
  return current;
}
} // namespace LunaDash
