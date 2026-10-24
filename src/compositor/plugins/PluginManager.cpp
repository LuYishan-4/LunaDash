#include "compositor/plugins/PluginManager.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "core/crypto/Sha256.hpp"
#include "core/plugins/PluginApi.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <glib.h>
#include <QUrl>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string_view>

namespace LunaDash {
namespace {
class PluginTemporaryDirectory final {
public:
  ~PluginTemporaryDirectory() {
    if (!path_.empty()) {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }
  }

  bool create(const std::filesystem::path &pattern) {
    if (!path_.empty())
      return false;
    auto buffer = pattern.string();
    buffer.push_back('\0');
    const auto *created = ::mkdtemp(buffer.data());
    if (!created)
      return false;
    path_ = created;
    return true;
  }

  bool isValid() const noexcept { return !path_.empty(); }

  const std::filesystem::path &path() const noexcept { return path_; }

  QString filePath(const QString &name) const {
    return QString::fromStdString((path_ / name.toStdString()).string());
  }

private:
  std::filesystem::path path_;
};
} // namespace

struct PluginManager::Native {
  // Load native code from an immutable private path. Some libc/loader
  // implementations keep mappings alive after dlclose(); mutating the
  // installed .so in place would then invalidate executable pages.
  // Keep the staged shared object immutable for the lifetime of the mapping.
  PluginTemporaryDirectory stage;
  void *library = nullptr;
  PluginDescriptor descriptor;
  PluginDescriptor current;
  const ludash_plugin_api *api = nullptr;

  ~Native() {
    if (library)
      ::dlclose(library);
  }
};

struct PluginManager::StoreInstall {
  QString id;
  QJsonObject item;
  QJsonArray files;
  std::unique_ptr<PluginTemporaryDirectory> stage;
  QString buildDirectory;
  QString installPrefix;
  QString cmakeExecutable;
  std::unique_ptr<ProcessRunner> process;
  int index = 0;
  int buildStep = 0; // 0 configure, 1 build, 2 install
  QString phase = "downloading";
};

struct PluginManager::DeferredCallback {
  PluginManager *manager = nullptr;
  wl_event_source *source = nullptr;
  std::function<void()> callback;
};

namespace {
bool safeInstallPath(const QString &value) {
  if (value.isEmpty() || value.size() > 180 || value.contains('\\'))
    return false;
  const auto path = value.toStdString();
  if (!std::all_of(path.begin(), path.end(), [](unsigned char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '_' ||
               character == '-' || character == '.' || character == '/';
      }))
    return false;
  const auto parts = value.split('/');
  return std::none_of(parts.cbegin(), parts.cend(),
                      [](const QString &part) {
                        return part.isEmpty() || part == "." || part == "..";
                      });
}

bool validPluginId(const QString &id) {
  const auto value = id.toStdString();
  const auto alphanumeric = [](unsigned char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9');
  };
  return value.size() >= 2 && alphanumeric(value.front()) &&
         std::all_of(value.begin() + 1, value.end(),
                     [&](unsigned char character) {
                       return alphanumeric(character) || character == '.' ||
                              character == '_' || character == '-';
                     });
}

bool validSha256(const QString &value) {
  const auto hash = value.toStdString();
  return hash.size() == 64 &&
         std::all_of(hash.begin(), hash.end(), [](unsigned char character) {
           return (character >= 'a' && character <= 'f') ||
                  (character >= 'A' && character <= 'F') ||
                  (character >= '0' && character <= '9');
         });
}

class HookStateGuard final {
public:
  explicit HookStateGuard(bool &state) : state_(state), previous_(state) {
    state_ = true;
  }
  ~HookStateGuard() { state_ = previous_; }

private:
  bool &state_;
  bool previous_;
};

QString userPluginRoot() {
  return QString::fromUtf8(g_get_user_data_dir()) + "/lunadash/plugins";
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
PluginManager::PluginManager(wl_event_loop *eventLoop) : eventLoop_(eventLoop) {
  QFile bundled(QStringLiteral(LUDASH_PLUGIN_CATALOG_SOURCE_FILE));
  bool catalogOpened = bundled.open(QIODevice::ReadOnly);
  if (!catalogOpened) {
    bundled.setFileName(QStringLiteral(LUDASH_PLUGIN_CATALOG_INSTALL_FILE));
    catalogOpened = bundled.open(QIODevice::ReadOnly);
  }
  if (!catalogOpened) {
    storeError_ = "Could not read the bundled plugin catalogue.";
  } else {
    QString error;
    if (!applyStoreCatalog(bundled.readAll(), &error))
      storeError_ = error;
  }
  refreshStore();
}
PluginManager::~PluginManager() {
  for (const auto &callback : deferredCallbacks_)
    if (callback->source)
      wl_event_source_remove(callback->source);
}

void PluginManager::setChangedCallback(std::function<void()> callback) {
  changedCallback_ = std::move(callback);
}

void PluginManager::notifyChanged() {
  snapshotDirty_ = true;
  if (changedCallback_)
    changedCallback_();
}

bool PluginManager::defer(std::function<void()> callback) {
  if (!eventLoop_ || !callback)
    return false;
  auto deferred = std::make_unique<DeferredCallback>();
  deferred->manager = this;
  deferred->callback = std::move(callback);
  auto *raw = deferred.get();
  deferredCallbacks_.push_back(std::move(deferred));
  raw->source = wl_event_loop_add_timer(eventLoop_, handleDeferredTimer, raw);
  if (raw->source && wl_event_source_timer_update(raw->source, 0) >= 0)
    return true;
  if (raw->source)
    wl_event_source_remove(raw->source);
  deferredCallbacks_.pop_back();
  return false;
}

int PluginManager::handleDeferredTimer(void *data) {
  auto *pending = static_cast<DeferredCallback *>(data);
  if (pending && pending->manager)
    pending->manager->dispatchDeferred(pending);
  return 0;
}

void PluginManager::dispatchDeferred(DeferredCallback *callback) {
  if (!callback)
    return;
  if (callback->source)
    wl_event_source_remove(callback->source);
  auto found = std::find_if(
      deferredCallbacks_.begin(), deferredCallbacks_.end(),
      [callback](const auto &entry) { return entry.get() == callback; });
  if (found == deferredCallbacks_.end())
    return;
  auto invoke = std::move(callback->callback);
  deferredCallbacks_.erase(found);
  invoke();
}

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

  QJsonArray normalized;
  std::set<QString> ids;
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
      std::set<QString> targetIds;
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
    if (!validPluginId(id) || ids.contains(id) || name.isEmpty() ||
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
      std::set<QString> paths;
      bool metadata = false;
      bool cmakeProject = false;
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
            !validSha256(sha)) {
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
    notifyChanged();
    return;
  }

  if (!eventLoop_) {
    storeError_ = "Plugin catalogue refresh requires the Wayland event loop";
    notifyChanged();
    return;
  }
  catalogRunner_ = std::make_unique<ProcessRunner>(eventLoop_);
  storeLoading_ = true;
  storeError_.clear();
  snapshotDirty_ = true;
  const std::vector<std::string> arguments{
      "--fail", "--location", "--proto", "=https", "--proto-redir", "=https",
      "--max-redirs", "5", "--max-filesize", "262144", "--silent",
      "--show-error", "--", url.toString().toStdString()};
  const bool started = catalogRunner_->runDetailed(
      "curl", arguments,
      [this](const ProcessResult &result) {
        if (!defer([this, result] {
              storeLoading_ = false;
              QString error;
              if (!result.success) {
                storeError_ =
                    "Remote catalogue unavailable; showing bundled catalogue";
              } else if (!applyStoreCatalog(
                             QByteArray::fromStdString(result.output), &error)) {
                storeError_ = error;
              } else {
                storeError_.clear();
              }
              notifyChanged();
            })) {
          storeLoading_ = false;
          storeError_ = "Could not schedule plugin catalogue processing";
          notifyChanged();
        }
      },
      30000);
  if (!started) {
    storeLoading_ = false;
    storeError_ = "Could not start plugin catalogue download";
    notifyChanged();
  }
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
  job->stage = std::make_unique<PluginTemporaryDirectory>();
  if (!job->stage->create(
          std::filesystem::path(root.toStdString()) /
          (".install-" + id.toStdString() + "-XXXXXX"))) {
    if (error)
      *error = "Could not create a plugin staging directory";
    return false;
  }

  auto *raw = job.get();
  storeInstallErrors_.erase(id);
  storeInstalls_.push_back(std::move(job));
  notifyChanged();
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
  const auto path = job->stage->filePath(relativePath);
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    finishStoreInstall(job, "Could not create the plugin staging path");
    return;
  }
  job->process = std::make_unique<ProcessRunner>(eventLoop_, false);
  const std::vector<std::string> arguments{
      "--fail", "--location", "--proto", "=https", "--proto-redir", "=https",
      "--max-redirs", "5", "--max-filesize", "4194304", "--silent",
      "--show-error", "--output", path.toStdString(), "--",
      url.toString().toStdString()};
  const bool started = job->process->runDetailed(
      "curl", arguments,
      [this, job, relativePath, expectedSha, path](const ProcessResult &result) {
        if (!defer([this, job, relativePath, expectedSha, path, result] {
              if (!result.success) {
                finishStoreInstall(job, "Plugin download failed");
                return;
              }
              std::ifstream input(path.toStdString(), std::ios::binary);
              if (!input || QFileInfo(path).size() > 4 * 1024 * 1024) {
                finishStoreInstall(job, "Could not read downloaded plugin file");
                return;
              }
              std::string data((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
              if (input.bad() || data.size() > 4 * 1024 * 1024) {
                finishStoreInstall(job, "Plugin file exceeds the 4 MiB limit");
                return;
              }
              const auto actual =
                  QString::fromStdString(sha256Hex(std::string_view(data)));
              if (actual != expectedSha) {
                finishStoreInstall(job,
                                   "Plugin file hash verification failed");
                return;
              }
              if (!QFile::setPermissions(
                      path, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
                finishStoreInstall(job, "Could not secure the staged plugin file");
                return;
              }
              ++job->index;
              continueStoreInstall(job);
            })) {
          finishStoreInstall(job,
                             "Could not schedule plugin download processing");
        }
      },
      60000);
  if (!started)
    finishStoreInstall(job, "Could not start plugin download");
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

  const auto findExecutable = [](const char *name) {
    gchar *path = g_find_program_in_path(name);
    const QString result = path ? QString::fromUtf8(path) : QString{};
    g_free(path);
    return result;
  };
  job->cmakeExecutable = findExecutable("cmake");
  const auto ninja = findExecutable("ninja");
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

  std::vector<std::string> arguments;
  if (job->buildStep == 0) {
    arguments = {
        "-S", job->stage->path().string(),
        "-B", job->buildDirectory.toStdString(),
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_PREFIX=" + job->installPrefix.toStdString(),
    };
    job->phase = "configuring";
  } else if (job->buildStep == 1) {
    arguments = {"--build", job->buildDirectory.toStdString(), "--parallel",
                 "2"};
    job->phase = "building";
  } else {
    arguments = {"--install", job->buildDirectory.toStdString()};
    job->phase = "installing";
  }

  job->process = std::make_unique<ProcessRunner>(eventLoop_, false);
  notifyChanged();
  const bool started = job->process->runDetailed(
      job->cmakeExecutable.toStdString(), arguments,
      [this, job](const ProcessResult &result) {
        if (!defer([this, job, result] {
              if (!result.success) {
                finishStoreInstall(
                    job, "Plugin SDK " + job->phase + " step failed");
                return;
              }
              if (job->buildStep < 2) {
                ++job->buildStep;
                runStoreBuildStep(job);
                return;
              }
              installBuiltStorePackage(job);
            })) {
          finishStoreInstall(job, "Could not schedule plugin SDK build result");
        }
      },
      0);
  if (!started)
    finishStoreInstall(job, "Could not start the plugin SDK build tool");
}

void PluginManager::installBuiltStorePackage(StoreInstall *job) {
  if (!job)
    return;

  const auto builtDirectory =
      QDir(job->installPrefix)
          .filePath("share/lunadash/plugins/" + job->id);
  const auto metadataPath = QDir(builtDirectory).filePath("metadata.json");
  const auto descriptors = readPluginMetadataTargets(metadataPath, false);
  if (descriptors.empty() ||
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
      QString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count());
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
        QString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count());
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
    notifyChanged();
    return;
  }
  storeInstallErrors_.erase(id);
  refresh();
}


bool PluginManager::removeInstalledPlugin(const QString &id, QString *error) {
  if (!validPluginId(id)) {
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
  if (descriptors.empty() ||
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
  storeInstallErrors_.erase(id);
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
  std::vector<PluginDescriptor> catalog;
  std::set<QString> active;
  std::set<QString> selectedTargets;

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
      errors_.erase(key);

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
      catalog.push_back(descriptor);
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

        native->stage.create(std::filesystem::path(g_get_tmp_dir()) /
                             "lunadash-plugin-XXXXXX");
        const QString stagedLibrary = native->stage.filePath("plugin.so");
        QFile sourceLibrary(descriptor.libraryPath);
        if (!native->stage.isValid()) {
          errors_[key] = "Could not create native plugin staging directory";
        } else if (!sourceLibrary.copy(stagedLibrary)) {
          errors_[key] = sourceLibrary.errorString();
        } else {
          native->library =
              ::dlopen(stagedLibrary.toStdString().c_str(), RTLD_NOW | RTLD_LOCAL);
          const char *openError = native->library ? nullptr : ::dlerror();
          const std::string libraryError =
              openError ? openError : "Could not load the plugin library.";
          using Entry = const ludash_plugin_api *(*)();
          ::dlerror();
          const auto symbol =
              native->library
                  ? ::dlsym(native->library, "ludash_plugin_entry_v2")
                  : nullptr;
          const char *loadError = ::dlerror();
          const auto entry = reinterpret_cast<Entry>(symbol);
          if (!native->library || loadError || !entry) {
            errors_[key] = native->library && loadError
                               ? QString::fromUtf8(loadError)
                               : QString::fromStdString(libraryError);
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

    catalog.push_back(descriptor);
  }

  std::erase_if(native_, [&](const auto &entry) {
    return !active.contains(pluginInstanceId(entry->descriptor));
  });

  for (auto it = errors_.begin(); it != errors_.end();) {
    const bool enabled =
        std::any_of(catalog.cbegin(), catalog.cend(), [&](const auto &plugin) {
          return pluginInstanceId(plugin) == it->first && plugin.enabled;
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
  notifyChanged();
}
QJsonObject PluginManager::snapshot() {
  if (!snapshotDirty_)
    return snapshotCache_;

  QJsonArray installed;
  std::set<QString> replacements;
  std::map<QString, QJsonObject> storeItems;
  for (const auto &value : storeCatalog_) {
    const auto item = value.toObject();
    storeItems[item.value("id").toString()] = item;
  }
  const auto removableRoot = QFileInfo(userPluginRoot()).canonicalFilePath();
  static const std::set<QString> retiredBundledIds{
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
    const auto storeItemIt = storeItems.find(descriptor.id);
    const QJsonObject storeItem = storeItemIt == storeItems.end()
                                      ? QJsonObject{}
                                      : storeItemIt->second;
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
    if (error.isEmpty()) {
      const auto errorIt = errors_.find(pluginInstanceId(descriptor));
      if (errorIt != errors_.end())
        error = errorIt->second;
    }
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
  std::set<QString> installedIds;
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
    const auto installError = storeInstallErrors_.find(id);
    item["installError"] =
        installError == storeInstallErrors_.end() ? QString()
                                                   : installError->second;
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
    notifyChanged();
    return;
  }

  errors_.erase(key);
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
  HookStateGuard guard(inHook_);
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
