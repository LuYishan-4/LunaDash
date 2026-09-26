#include "compositor/plugins/PluginManager.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "core/plugins/PluginApi.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLibrary>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QSet>
#include <QStandardPaths>
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

struct PluginManager::StoreInstall {
  QString id;
  QJsonObject item;
  QJsonArray files;
  std::unique_ptr<QTemporaryDir> stage;
  QString buildDirectory;
  QString installPrefix;
  QString cmakeExecutable;
  QProcess *process = nullptr;
  QByteArray processTail;
  int index = 0;
  int buildStep = 0; // 0 configure, 1 build, 2 install
  QString phase = "downloading";
};

namespace {
bool safeInstallPath(const QString &value) {
  static const QRegularExpression pattern(
      R"(^(?:[A-Za-z0-9_.-]+/)*[A-Za-z0-9_.-]+$)");
  if (value.isEmpty() || value.size() > 180 || value.contains('\\') ||
      !pattern.match(value).hasMatch())
    return false;
  const auto parts = value.split('/');
  return std::none_of(parts.cbegin(), parts.cend(),
                      [](const QString &part) {
                        return part.isEmpty() || part == "." || part == "..";
                      });
}

QString userPluginRoot() {
  const auto data =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  return data.isEmpty() ? QString() : data + "/lunadash/plugins";
}

bool directoryClaimsPluginId(const QString &directory, const QString &id) {
  for (const auto &name : {QString("metadata.json"), QString("manifest.json")}) {
    QFile file(QDir(directory).filePath(name));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
      continue;
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    if (object.isEmpty())
      continue;
    const auto directId = object.value("id").toString();
    const auto kpluginId =
        object.value("KPlugin").toObject().value("Id").toString();
    if (directId == id || kpluginId == id)
      return true;
  }
  return false;
}
} // namespace
PluginManager::PluginManager(QObject *parent) : QObject(parent) {
  connect(this, &PluginManager::changed, this,
          [this] { snapshotDirty_ = true; });
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

    if (item.contains("install")) {
      const auto install = item.value("install");
      if (!install.isObject()) {
        if (error)
          *error = "Plugin install payload must be an object";
        return false;
      }
      const auto files = install.toObject().value("files");
      if (!files.isArray() || files.toArray().isEmpty() ||
          files.toArray().size() > 32) {
        if (error)
          *error = "Plugin install file list is invalid";
        return false;
      }
      QSet<QString> paths;
      bool metadata = false;
      bool cmakeProject = false;
      static const QRegularExpression shaPattern("^[0-9a-fA-F]{64}$");
      for (const auto &fileValue : files.toArray()) {
        if (!fileValue.isObject()) {
          if (error)
            *error = "Plugin install entries must be objects";
          return false;
        }
        const auto file = fileValue.toObject();
        const auto path = file.value("path").toString();
        const QUrl url(file.value("url").toString());
        const auto sha = file.value("sha256").toString();
        if (!safeInstallPath(path) || paths.contains(path) ||
            !url.isValid() || url.scheme() != "https" ||
            !shaPattern.match(sha).hasMatch()) {
          if (error)
            *error = "Plugin install entry is invalid";
          return false;
        }
        metadata = metadata || path == "metadata.json";
        cmakeProject = cmakeProject || path == "CMakeLists.txt";
        paths.insert(path);
      }
      if (!metadata || !cmakeProject) {
        if (error)
          *error = "Plugin install package must include metadata.json and CMakeLists.txt";
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
  snapshotDirty_ = true;
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


bool PluginManager::installFromStore(const QString &id, QString *error) {
  if (id.isEmpty()) {
    if (error)
      *error = "Plugin id is required";
    return false;
  }
  if (std::any_of(storeInstalls_.cbegin(), storeInstalls_.cend(),
                  [&](const auto &job) { return job->id == id; })) {
    if (error)
      *error = "Plugin download is already running";
    return false;
  }

  QJsonObject item;
  for (const auto &value : storeCatalog_) {
    const auto candidate = value.toObject();
    if (candidate.value("id").toString() == id) {
      item = candidate;
      break;
    }
  }
  if (item.isEmpty()) {
    if (error)
      *error = "Plugin is not in the reviewed store catalogue";
    return false;
  }
  const auto install = item.value("install").toObject();
  const auto files = install.value("files").toArray();
  if (install.isEmpty() || files.isEmpty()) {
    if (error)
      *error = "This plugin is source-only and cannot be downloaded directly";
    return false;
  }

  const auto discovered = discoverPlugins();
  bool installedPackageFound = false;
  for (const auto &plugin : discovered) {
    if (plugin.id != id)
      continue;
    installedPackageFound = true;
    if (pluginMatchesStore(plugin, item)) {
      if (error)
        *error = "Plugin is already installed";
      return false;
    }
    // Revisions may share a product version; compare the complete Store source
    // fingerprint. Downloads are still staged and validated before replacement.
    break;
  }

  const auto root = userPluginRoot();
  if (root.isEmpty() || !QDir().mkpath(root)) {
    if (error)
      *error = "Could not create the user plugin directory";
    return false;
  }
  const auto destination = QDir(root).filePath(id);
  if (QFileInfo::exists(destination) && !installedPackageFound) {
    // Older LunaDash releases could leave a same-id package in the canonical
    // user directory without a current SDK receipt, so discoverPlugins() may
    // not classify it as an installed SDK2 package. If its old metadata still
    // proves ownership, treat it as an outdated install and let the verified
    // Store build replace it. Unknown directories are never overwritten.
    if (!directoryClaimsPluginId(destination, id)) {
      if (error)
        *error = "Plugin destination already exists";
      return false;
    }
    installedPackageFound = true;
  }

  auto job = std::make_unique<StoreInstall>();
  job->id = id;
  job->item = item;
  job->files = files;
  job->stage = std::make_unique<QTemporaryDir>(
      QDir(root).filePath(".install-" + id + "-XXXXXX"));
  if (!job->stage->isValid()) {
    if (error)
      *error = "Could not create a plugin staging directory";
    return false;
  }

  auto *raw = job.get();
  storeInstallErrors_.remove(id);
  storeInstalls_.push_back(std::move(job));
  emit changed();
  continueStoreInstall(raw);
  if (error)
    error->clear();
  return true;
}

void PluginManager::continueStoreInstall(StoreInstall *job) {
  if (!job)
    return;

  if (job->index >= job->files.size()) {
    startStoreBuild(job);
    return;
  }

  const auto entry = job->files.at(job->index).toObject();
  const auto relativePath = entry.value("path").toString();
  const auto expectedSha = entry.value("sha256").toString().toLower();
  const QUrl url(entry.value("url").toString());
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  auto *reply = storeNetwork_->get(request);
  connect(reply, &QNetworkReply::finished, this,
          [this, job, reply, relativePath, expectedSha] {
            const auto failure = reply->error();
            const auto data = reply->readAll();
            reply->deleteLater();
            if (failure != QNetworkReply::NoError) {
              finishStoreInstall(job, "Plugin download failed");
              return;
            }
            if (data.size() > 4 * 1024 * 1024) {
              finishStoreInstall(job, "Plugin file exceeds the 4 MiB limit");
              return;
            }
            const auto actual = QString::fromLatin1(
                QCryptographicHash::hash(data, QCryptographicHash::Sha256)
                    .toHex());
            if (actual != expectedSha) {
              finishStoreInstall(job, "Plugin file hash verification failed");
              return;
            }

            const auto path = job->stage->filePath(relativePath);
            if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
              finishStoreInstall(job, "Could not create the plugin staging path");
              return;
            }
            QSaveFile file(path);
            file.setDirectWriteFallback(false);
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() ||
                !file.commit()) {
              finishStoreInstall(job, "Could not write a downloaded plugin file");
              return;
            }
            QFile::setPermissions(path,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            ++job->index;
            continueStoreInstall(job);
          });
}


void PluginManager::startStoreBuild(StoreInstall *job) {
  if (!job)
    return;
  const auto cmakeFile = job->stage->filePath("CMakeLists.txt");
  const auto metadataFile = job->stage->filePath("metadata.json");
  if (!QFileInfo(cmakeFile).isFile() || !QFileInfo(metadataFile).isFile()) {
    finishStoreInstall(job, "Downloaded plugin is missing its SDK CMake project");
    return;
  }

  job->cmakeExecutable = QStandardPaths::findExecutable("cmake");
  const auto ninja = QStandardPaths::findExecutable("ninja");
  if (job->cmakeExecutable.isEmpty() || ninja.isEmpty()) {
    finishStoreInstall(
        job, "One-click plugin installation requires cmake and ninja");
    return;
  }

  job->buildDirectory = job->stage->filePath(".build");
  job->installPrefix = job->stage->filePath(".prefix");
  if (!QDir().mkpath(job->buildDirectory) ||
      !QDir().mkpath(job->installPrefix)) {
    finishStoreInstall(job, "Could not create plugin build directories");
    return;
  }

  job->phase = "configuring";
  job->buildStep = 0;
  runStoreBuildStep(job);
}

void PluginManager::runStoreBuildStep(StoreInstall *job) {
  if (!job)
    return;

  QStringList arguments;
  if (job->buildStep == 0) {
    arguments = {
        "-S", job->stage->path(),
        "-B", job->buildDirectory,
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_PREFIX=" + job->installPrefix,
    };
    job->phase = "configuring";
  } else if (job->buildStep == 1) {
    arguments = {"--build", job->buildDirectory, "--parallel", "2"};
    job->phase = "building";
  } else {
    arguments = {"--install", job->buildDirectory};
    job->phase = "installing";
  }

  job->processTail.clear();
  auto *process = new QProcess(this);
  job->process = process;
  process->setWorkingDirectory(job->stage->path());
  process->setProcessChannelMode(QProcess::MergedChannels);
  connect(process, &QProcess::readyRead, this, [job, process] {
    job->processTail.append(process->readAll());
    if (job->processTail.size() > 8192)
      job->processTail = job->processTail.right(8192);
  });
  connect(process, &QProcess::finished, this,
          [this, job, process](int code, QProcess::ExitStatus status) {
            job->processTail.append(process->readAll());
            if (job->processTail.size() > 8192)
              job->processTail = job->processTail.right(8192);
            job->process = nullptr;
            process->deleteLater();
            if (status != QProcess::NormalExit || code != 0) {
              auto detail = QString::fromLocal8Bit(job->processTail).trimmed();
              detail = detail.right(2048);
              finishStoreInstall(
                  job, QString("Plugin SDK %1 failed.%2")
                           .arg(job->phase,
                                detail.isEmpty() ? QString()
                                                 : " " + detail));
              return;
            }
            if (job->buildStep < 2) {
              ++job->buildStep;
              runStoreBuildStep(job);
              return;
            }
            installBuiltStorePackage(job);
          });
  connect(process, &QProcess::errorOccurred, this,
          [this, job, process](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
              return;
            job->process = nullptr;
            process->deleteLater();
            finishStoreInstall(job, "Could not start the plugin SDK build tool");
          });
  emit changed();
  process->start(job->cmakeExecutable, arguments);
}

void PluginManager::installBuiltStorePackage(StoreInstall *job) {
  if (!job)
    return;

  const auto builtDirectory =
      QDir(job->installPrefix)
          .filePath("share/lunadash/plugins/" + job->id);
  const auto metadataPath = QDir(builtDirectory).filePath("metadata.json");
  const auto descriptors = readPluginMetadataTargets(metadataPath, false);
  if (descriptors.isEmpty() ||
      std::any_of(descriptors.cbegin(), descriptors.cend(),
                  [&](const auto &descriptor) {
                    return descriptor.id != job->id ||
                           !descriptor.error.isEmpty();
                  })) {
    finishStoreInstall(
        job, "Built plugin failed LunaDash SDK/runtime validation");
    return;
  }

  QSaveFile receipt(QDir(builtDirectory).filePath(".lunadash-store.json"));
  receipt.setDirectWriteFallback(false);
  const auto receiptBytes = QJsonDocument(QJsonObject{
      {"fingerprint", pluginStoreFingerprint(job->item)}}).toJson(QJsonDocument::Compact);
  if (!receipt.open(QIODevice::WriteOnly) ||
      receipt.write(receiptBytes) != receiptBytes.size() || !receipt.commit()) {
    finishStoreInstall(job, "Could not write the Store revision receipt");
    return;
  }

  const auto root = userPluginRoot();
  const auto destination = QDir(root).filePath(job->id);
  const auto backupName =
      ".old-" + job->id + "-" +
      QString::number(QDateTime::currentMSecsSinceEpoch());
  const auto backup = QDir(root).filePath(backupName);

  auto configDocument = readExtensionConfiguration();
  auto configs = configDocument.value("plugins").toObject();
  configs.remove(job->id);
  configDocument["plugins"] = configs;
  QString configError;
  if (!saveExtensionConfiguration(QJsonDocument(configDocument).toJson(),
                                  &configError)) {
    finishStoreInstall(
        job, configError.isEmpty() ? "Could not reset the plugin configuration"
                                    : configError);
    return;
  }
  refresh();

  bool backedUp = false;
  if (QFileInfo::exists(destination)) {
    if (!QDir(root).rename(job->id, backupName)) {
      finishStoreInstall(job, "Could not stage the existing plugin for update");
      return;
    }
    backedUp = true;
  }

  if (!QDir(root).rename(
          QDir(job->installPrefix).relativeFilePath(builtDirectory),
          job->id)) {
    // QDir::rename above is relative to root, so fall back to moving the
    // built package via an intermediate directory in the same filesystem.
    const auto stagedName =
        ".built-" + job->id + "-" +
        QString::number(QDateTime::currentMSecsSinceEpoch());
    const auto stagedPath = QDir(root).filePath(stagedName);
    if (!QDir().rename(builtDirectory, stagedPath) ||
        !QDir(root).rename(stagedName, job->id)) {
      if (backedUp)
        QDir(root).rename(backupName, job->id);
      finishStoreInstall(job, "Could not install the built plugin package");
      return;
    }
  }

  if (backedUp)
    QDir(backup).removeRecursively();
  finishStoreInstall(job, {});
}

void PluginManager::finishStoreInstall(StoreInstall *job,
                                       const QString &error) {
  if (!job)
    return;
  const auto id = job->id;
  std::erase_if(storeInstalls_,
                [job](const auto &entry) { return entry.get() == job; });
  if (!error.isEmpty()) {
    storeInstallErrors_[id] = error.left(1024);
    emit changed();
    return;
  }
  storeInstallErrors_.remove(id);
  refresh();
}


bool PluginManager::removeInstalledPlugin(const QString &id, QString *error) {
  static const QRegularExpression validId("^[A-Za-z0-9][A-Za-z0-9._-]+$");
  if (!validId.match(id).hasMatch()) {
    if (error)
      *error = "Invalid plugin id";
    return false;
  }

  const auto root = userPluginRoot();
  const auto rootCanonical = QFileInfo(root).canonicalFilePath();
  const auto destination = QDir(root).filePath(id);
  const auto destinationCanonical = QFileInfo(destination).canonicalFilePath();
  if (rootCanonical.isEmpty() || destinationCanonical.isEmpty() ||
      QFileInfo(destinationCanonical).absolutePath() != rootCanonical) {
    if (error)
      *error = "Only user-installed plugins can be removed";
    return false;
  }

  const auto metadataPath = QDir(destinationCanonical).filePath("metadata.json");
  const auto descriptors = readPluginMetadataTargets(metadataPath, false);
  if (descriptors.isEmpty() ||
      std::none_of(descriptors.cbegin(), descriptors.cend(),
                   [&](const auto &descriptor) { return descriptor.id == id; })) {
    if (error)
      *error = "Installed plugin metadata does not match the requested id";
    return false;
  }

  auto document = readExtensionConfiguration();
  auto plugins = document.value("plugins").toObject();
  plugins.remove(id);
  document["plugins"] = plugins;
  QString configError;
  if (!saveExtensionConfiguration(QJsonDocument(document).toJson(), &configError)) {
    if (error)
      *error = configError.isEmpty() ? "Could not disable the plugin before removal"
                                     : configError;
    return false;
  }

  // Refresh first so native modules from this package are unloaded/staged
  // independently before any user files are removed.
  refresh();
  if (!QDir(destinationCanonical).removeRecursively()) {
    if (error)
      *error = "Could not remove the user plugin directory";
    return false;
  }
  storeInstallErrors_.remove(id);
  refresh();
  if (error)
    error->clear();
  return true;
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
  if (!snapshotDirty_)
    return snapshotCache_;

  QJsonArray installed;
  QSet<QString> replacements;
  QHash<QString, QJsonObject> storeItems;
  for (const auto &value : storeCatalog_) {
    const auto item = value.toObject();
    storeItems.insert(item.value("id").toString(), item);
  }
  const auto removableRoot = QFileInfo(userPluginRoot()).canonicalFilePath();
  static const QSet<QString> retiredBundledIds{
      "org.ludash.fade",
      "org.lunadash.stacking-windows",
  };
  for (const auto &descriptor : catalog_) {
    // The modern settings surface is SDK 2 only. Legacy schema-1/native
    // descriptors remain readable for migration but are intentionally hidden.
    if (descriptor.schemaVersion != 2 ||
        retiredBundledIds.contains(descriptor.id))
      continue;
    auto item = pluginDescriptorJson(descriptor);
    const auto storeItem = storeItems.value(descriptor.id);
    const auto storeVersion = storeItem.value("version").toString();
    item["outdated"] =
        !storeVersion.isEmpty() && !pluginMatchesStore(descriptor, storeItem);
    item["storeVersion"] = storeVersion;
    const auto packageDir =
        QFileInfo(descriptor.metadataPath).absoluteDir().canonicalPath();
    item["removable"] =
        !removableRoot.isEmpty() && !packageDir.isEmpty() &&
        QFileInfo(packageDir).absolutePath() == removableRoot;
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
    const auto id = item.value("id").toString();
    QString installedVersion;
    bool current = false;
    for (const auto &descriptor : catalog_) {
      if (descriptor.id == id && descriptor.schemaVersion == 2) {
        installedVersion = descriptor.version;
        current = pluginMatchesStore(descriptor, item);
        break;
      }
    }
    const bool installed = !installedVersion.isEmpty();
    item["installed"] = current;
    item["installedVersion"] = installedVersion;
    item["updateAvailable"] = installed && !current;
    item["installable"] = item.value("install").isObject();
    item["installing"] =
        std::any_of(storeInstalls_.cbegin(), storeInstalls_.cend(),
                    [&](const auto &job) { return job->id == id; });
    for (const auto &job : storeInstalls_) {
      if (job->id == id) {
        item["installPhase"] = job->phase;
        break;
      }
    }
    item["installError"] = storeInstallErrors_.value(id);
    remote.append(item);
  }
  snapshotCache_ = {{"installed", installed},
                    {"remote", remote},
                    {"targets", targets},
                    {"document", document},
                    {"path", extensionConfigurationPath()},
                    {"error", configError},
                    {"storeSupported", true},
                    {"storeLoading", storeLoading_},
                    {"storeError", storeError_}};
  snapshotDirty_ = false;
  return snapshotCache_;
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
