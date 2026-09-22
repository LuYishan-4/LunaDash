#include "compositor/plugins/PluginManager.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "core/plugins/PluginApi.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLibrary>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <algorithm>

namespace LunaDash {
struct PluginManager::Native {
  // Load native code from an immutable private path. Some libc/loader
  // implementations keep mappings alive after dlclose(); mutating the
  // installed .so in place would then invalidate executable pages.
  // QTemporaryDir is declared before QLibrary so the library is destroyed
  // before its staged file is unlinked.
  QTemporaryDir stage;
  QLibrary library;
  PluginDescriptor descriptor;
  PluginDescriptor current;
  const ludash_plugin_api *api = nullptr;
};
PluginManager::PluginManager(QObject *parent) : QObject(parent) {
  QFile bundled(":/LunaDash/plugins/catalog.json");
  if (bundled.open(QIODevice::ReadOnly)) {
    QString error;
    if (!applyStoreCatalog(bundled.readAll(), &error))
      storeError_ = error;
  }
  storeNetwork_ = new QNetworkAccessManager(this);
  QTimer::singleShot(0, this, &PluginManager::refreshStore);
}
PluginManager::~PluginManager() = default;

bool PluginManager::applyStoreCatalog(const QByteArray &bytes, QString *error) {
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    if (error)
      *error = "Invalid plugin catalogue JSON";
    return false;
  }
  const auto root = document.object();
  const auto values = root.value("plugins");
  if (root.value("schemaVersion").toInt() != 1 ||
      (root.contains("format") &&
       root.value("format").toString() != "lunadash-plugin-index") ||
      !values.isArray() || values.toArray().size() > 256) {
    if (error)
      *error = "Unsupported or oversized plugin catalogue";
    return false;
  }

  static const QRegularExpression validId("^[A-Za-z0-9][A-Za-z0-9._-]+$");
  QJsonArray normalized;
  QSet<QString> ids;
  for (const auto &value : values.toArray()) {
    if (!value.isObject()) {
      if (error)
        *error = "Plugin catalogue entries must be objects";
      return false;
    }
    auto item = value.toObject();
    const auto id = item.value("id").toString();
    const auto name = item.value("name").toString();
    const auto version = item.value("version").toString();
    bool validTargets = false;
    if (item.value("targets").isArray()) {
      const auto targetItems = item.value("targets").toArray();
      QSet<QString> targetIds;
      validTargets = !targetItems.isEmpty() && targetItems.size() <= 16;
      for (const auto &targetValue : targetItems) {
        const auto implementation = targetValue.toObject();
        const auto targetId = implementation.value("target").toString(
            implementation.value("id").toString());
        const auto type = implementation.value("type").toString();
        const auto target = extensionTarget(targetId);
        if (!targetValue.isObject() || targetId.isEmpty() ||
            targetIds.contains(targetId) || target.isEmpty() ||
            !target.value("types").toArray().contains(type)) {
          validTargets = false;
          break;
        }
        targetIds.insert(targetId);
      }
    } else {
      const auto type = item.value("type").toString();
      const auto targetId = item.value("target").toString();
      const auto target = extensionTarget(targetId);
      validTargets =
          !target.isEmpty() && target.value("types").toArray().contains(type);
    }
    if (!validId.match(id).hasMatch() || ids.contains(id) || name.isEmpty() ||
        version.isEmpty() || !validTargets) {
      if (error)
        *error = "Plugin catalogue contains an invalid identity or target";
      return false;
    }
    ids.insert(id);

    const auto tags = item.value("tags");
    const auto tagArray = tags.toArray();
    if (tags.isUndefined())
      item["tags"] = QJsonArray{};
    else if (!tags.isArray() || tags.toArray().size() > 12 ||
             std::any_of(tagArray.cbegin(), tagArray.cend(),
                         [](const QJsonValue &tag) {
                           const auto text = tag.toString();
                           return !tag.isString() || text.trimmed() != text ||
                                  text.isEmpty() || text.size() > 32;
                         })) {
      if (error)
        *error = "Plugin catalogue contains invalid tags";
      return false;
    }

    const auto icon = item.value("icon").toString("applications-system");
    const QUrl iconUrl(icon);
    if (icon.contains('/') &&
        (!iconUrl.isValid() || iconUrl.scheme() != "https")) {
      if (error)
        *error = "Remote plugin icons must use HTTPS";
      return false;
    }
    item["icon"] = icon;

    if (item.contains("sourceUrl")) {
      const QUrl source(item.value("sourceUrl").toString());
      if (!source.isValid() || source.scheme() != "https") {
        if (error)
          *error = "Plugin source URLs must use HTTPS";
        return false;
      }
    }
    normalized.append(item);
  }
  storeCatalog_ = normalized;
  if (error)
    error->clear();
  return true;
}

void PluginManager::refreshStore() {
  const QString configured = qEnvironmentVariable(
      "LUNADASH_PLUGIN_CATALOG_URL",
      "https://raw.githubusercontent.com/LuYishan-4/LunaDash-Plugins/main/index.json");
  if (configured.isEmpty() || configured.compare("off", Qt::CaseInsensitive) == 0)
    return;

  const QUrl url(configured);
  if (!url.isValid() || url.scheme() != "https") {
    storeError_ = "Plugin catalogue URL must use HTTPS";
    emit changed();
    return;
  }

  storeLoading_ = true;
  storeError_.clear();
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  auto *reply = storeNetwork_->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    storeLoading_ = false;
    QString error;
    if (reply->error() != QNetworkReply::NoError) {
      storeError_ = "Remote catalogue unavailable; showing bundled catalogue";
    } else if (!applyStoreCatalog(reply->readAll(), &error)) {
      storeError_ = error;
    } else {
      storeError_.clear();
    }
    reply->deleteLater();
    emit changed();
  });
}

void PluginManager::loadEnabled() { refresh(); }
void PluginManager::refresh() {
  if (inHook_)
    return;

  const auto discovered = discoverPlugins();
  QList<PluginDescriptor> catalog;
  QSet<QString> active;
  QSet<QString> selectedTargets;

  for (auto descriptor : discovered) {
    const auto id = descriptor.id;
    const auto key = pluginInstanceId(descriptor);
    const auto previous =
        std::find_if(catalog_.cbegin(), catalog_.cend(), [&](const auto &plugin) {
          return pluginInstanceId(plugin) == key;
        });
    if (previous == catalog_.cend() ||
        previous->manifest != descriptor.manifest ||
        previous->mode != descriptor.mode ||
        previous->settings != descriptor.settings ||
        previous->enabled != descriptor.enabled)
      errors_.remove(key);

    if (descriptor.enabled && descriptor.error.isEmpty() &&
        extensionTarget(descriptor.target)
                .value("selection")
                .toString("single") == "single") {
      if (selectedTargets.contains(descriptor.target))
        descriptor.error =
            "Another plugin is already enabled for this single-owner target";
      else
        selectedTargets.insert(descriptor.target);
    }

    if (!descriptor.enabled || !descriptor.error.isEmpty()) {
      catalog.append(descriptor);
      continue;
    }

    if (descriptor.type == "effect") {
      active.insert(key);
      auto found =
          std::find_if(native_.begin(), native_.end(), [&](const auto &entry) {
            return pluginInstanceId(entry->descriptor) == key;
          });
      if (found != native_.end() &&
          ((*found)->descriptor.manifest != descriptor.manifest ||
           (*found)->descriptor.libraryPath != descriptor.libraryPath)) {
        native_.erase(found);
        found = native_.end();
      }

      if (found != native_.end()) {
        (*found)->current = descriptor;
      } else if (!errors_.contains(key)) {
        auto native = std::make_unique<Native>();
        native->descriptor = native->current = descriptor;

        const QString stagedLibrary = native->stage.filePath("plugin.so");
        QFile sourceLibrary(descriptor.libraryPath);
        if (!native->stage.isValid()) {
          errors_[key] = "Could not create native plugin staging directory";
        } else if (!sourceLibrary.copy(stagedLibrary)) {
          errors_[key] = sourceLibrary.errorString();
        } else {
          native->library.setFileName(stagedLibrary);

          using Entry = const ludash_plugin_api *(*)();
          const auto entry = reinterpret_cast<Entry>(
              native->library.resolve("ludash_plugin_entry_v2"));
          if (!entry) {
            errors_[key] = native->library.errorString();
          } else {
            native->api = entry();
            const auto *api = native->api;
            if (!api || api->struct_size != sizeof(ludash_plugin_api) ||
                api->api_version != LUDASH_PLUGIN_API_VERSION || !api->process ||
                !api->metadata_json ||
                QJsonDocument::fromJson(api->metadata_json).object() !=
                    descriptor.manifest) {
              errors_[key] = "SDK ABI or embedded metadata mismatch";
            } else {
              native_.push_back(std::move(native));
            }
          }
        }
      }
    }

    catalog.append(descriptor);
  }

  std::erase_if(native_, [&](const auto &entry) {
    return !active.contains(pluginInstanceId(entry->descriptor));
  });

  for (auto it = errors_.begin(); it != errors_.end();) {
    const bool enabled =
        std::any_of(catalog.cbegin(), catalog.cend(), [&](const auto &plugin) {
          return pluginInstanceId(plugin) == it.key() && plugin.enabled;
        });
    if (!enabled)
      it = errors_.erase(it);
    else
      ++it;
  }

  std::sort(native_.begin(), native_.end(), [](const auto &a, const auto &b) {
    return a->descriptor.id < b->descriptor.id;
  });
  catalog_ = catalog;
  emit changed();
}
QJsonObject PluginManager::snapshot() {
  QJsonArray installed;
  QSet<QString> replacements;
  for (const auto &descriptor : catalog_) {
    auto item = pluginDescriptorJson(descriptor);
    QString error = descriptor.error;
    if (error.isEmpty())
      error = errors_.value(pluginInstanceId(descriptor));
    if (descriptor.enabled && error.isEmpty() && descriptor.mode == "replace") {
      if (replacements.contains(descriptor.target))
        error = "Another replacement is already selected for this target";
      replacements.insert(descriptor.target);
    }
    bool loaded = descriptor.type != "effect";
    if (!loaded)
      loaded =
          std::any_of(native_.begin(), native_.end(), [&](const auto &entry) {
            return pluginInstanceId(entry->descriptor) ==
                       pluginInstanceId(descriptor) &&
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
  QSet<QString> installedIds;
  for (const auto &descriptor : catalog_)
    installedIds.insert(descriptor.id);
  QJsonArray remote;
  for (const auto &value : storeCatalog_) {
    auto item = value.toObject();
    item["installed"] = installedIds.contains(item.value("id").toString());
    remote.append(item);
  }
  return {{"installed", installed},
          {"remote", remote},
          {"targets", targets},
          {"document", document},
          {"path", extensionConfigurationPath()},
          {"error", configError},
          {"storeSupported", true},
          {"storeLoading", storeLoading_},
          {"storeError", storeError_}};
}
bool PluginManager::setEnabled(const QString &id, bool enabled,
                               QString *error) {
  auto document = readExtensionConfiguration();
  auto configs = document.value("plugins").toObject();
  for (const auto &plugin : discoverPlugins()) {
    if (plugin.id != id)
      continue;
    QJsonObject package = configs.value(id).toObject();
    package["enabled"] = enabled;
    QJsonObject targets = package.value("targets").toObject();
    for (const auto &candidate : discoverPlugins()) {
      if (candidate.id != id)
        continue;
      targets[candidate.target] =
          QJsonObject{{"enabled", enabled},
                      {"mode", candidate.mode},
                      {"settings", candidate.settings}};
    }
    package.remove("mode");
    package.remove("settings");
    package["targets"] = targets;
    configs[id] = package;
    document["plugins"] = configs;
    return saveExtensionConfiguration(QJsonDocument(document).toJson(), error);
  }
  if (error)
    *error = "Unknown plugin";
  return false;
}
void PluginManager::reportError(const QString &key, const QString &error) {
  if (!error.isEmpty()) {
    errors_[key] = error.left(1024);
    emit changed();
    return;
  }

  errors_.remove(key);
  if (!inHook_)
    refresh();
}
QString PluginManager::windowTemplateKey() const {
  for (const auto &native : native_) {
    const auto &plugin = native->current;
    if (plugin.enabled && plugin.error.isEmpty() &&
        !errors_.contains(pluginInstanceId(plugin)) &&
        plugin.manifest == native->descriptor.manifest &&
        plugin.target == "window-layout" && plugin.mode == "replace")
      return plugin.implementation.value("windowTemplate").toString("tiling");
  }
  return "tiling";
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
          errors_.contains(pluginInstanceId(descriptor)) ||
          (mode == "replace" && replaced))
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
        reportError(pluginInstanceId(descriptor), "Plugin threw across its C ABI");
        continue;
      }
      if (length < 0 || length >= response.size()) {
        reportError(pluginInstanceId(descriptor),
                    "Plugin returned an invalid response length");
        continue;
      }
      const auto result = QJsonDocument::fromJson(response.left(length));
      if (!result.isObject() || !validate(result.object())) {
        reportError(pluginInstanceId(descriptor),
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
