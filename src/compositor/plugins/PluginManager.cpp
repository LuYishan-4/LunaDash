#include "compositor/plugins/PluginManager.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"
#include "config/plugins/ExtensionRegistry.hpp"
#include "core/crypto/Sha256.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/filesystem/TemporaryDirectory.hpp"
#include "core/plugins/PluginApi.h"
#include "core/text/Text.hpp"
#include "core/text/Url.hpp"
#include <glib.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <map>
#include <set>
#include <string_view>

namespace LunaDash {

struct PluginManager::Native {
  // Load native code from an immutable private path. Some libc/loader
  // implementations keep mappings alive after dlclose(); mutating the
  // installed .so in place would then invalidate executable pages.
  // Keep the staged shared object immutable for the lifetime of the mapping.
  std::unique_ptr<TemporaryDirectory> stage;
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
  std::string id;
  Json item = Json::object();
  Json files = Json::array();
  std::unique_ptr<TemporaryDirectory> stage;
  std::string buildDirectory;
  std::string installPrefix;
  std::string cmakeExecutable;
  std::unique_ptr<ProcessRunner> process;
  std::size_t index = 0;
  int buildStep = 0; // 0 configure, 1 build, 2 install
  std::string phase = "downloading";
};

struct PluginManager::DeferredCallback {
  PluginManager *manager = nullptr;
  wl_event_source *source = nullptr;
  std::function<void()> callback;
};

namespace {
bool safeInstallPath(const std::string &value) {
  if (value.empty() || value.size() > 180 ||
      value.find('\\') != std::string::npos)
    return false;
  if (!std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '_' ||
               character == '-' || character == '.' || character == '/';
      }))
    return false;
  const auto parts = splitText(value, '/');
  return std::none_of(parts.cbegin(), parts.cend(),
                      [](const std::string &part) {
                        return part.empty() || part == "." || part == "..";
                      });
}

bool validPluginId(const std::string &value) {
  const auto alphanumeric = [](unsigned char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9');
  };
  return value.size() >= 2 &&
         alphanumeric(static_cast<unsigned char>(value.front())) &&
         std::all_of(value.begin() + 1, value.end(),
                     [&](unsigned char character) {
                       return alphanumeric(character) || character == '.' ||
                              character == '_' || character == '-';
                     });
}

bool validSha256(const std::string &hash) {
  return hash.size() == 64 &&
         std::all_of(hash.begin(), hash.end(), [](unsigned char character) {
           return (character >= 'a' && character <= 'f') ||
                  (character >= 'A' && character <= 'F') ||
                  (character >= '0' && character <= '9');
         });
}

bool arrayContains(const Json &array, const Json &value) {
  return array.is_array() &&
         std::find(array.begin(), array.end(), value) != array.end();
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

std::string userPluginRoot() {
  return std::string(g_get_user_data_dir()) + "/lunadash/plugins";
}

std::string canonicalPath(const std::filesystem::path &path) {
  std::error_code error;
  const auto result = std::filesystem::canonical(path, error);
  return error ? std::string() : result.string();
}

bool pathExists(const std::filesystem::path &path) {
  std::error_code error;
  return std::filesystem::exists(std::filesystem::symlink_status(path, error));
}

bool makeDirectories(const std::filesystem::path &path) {
  return createDirectories(path);
}

std::string millisecondsSinceEpoch() {
  return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count());
}

bool directoryClaimsPluginId(const std::filesystem::path &directory,
                             const std::string &id) {
  for (const auto *name : {"metadata.json", "manifest.json"}) {
    const auto contents = readFile(directory / name, 65536);
    if (!contents)
      continue;
    const auto object = parseJson(*contents);
    if (!object.is_object() || object.empty())
      continue;
    const auto directId = jsonString(object, "id");
    const auto kpluginId = jsonString(jsonObject(object, "KPlugin"), "Id");
    if (directId == id || kpluginId == id)
      return true;
  }
  return false;
}

bool removeExtensionPackageConfiguration(const std::string &id,
                                         std::string *error) {
  auto document = readExtensionConfiguration();
  auto plugins = jsonObject(document, "plugins");
  plugins.erase(id);
  document["plugins"] = plugins;
  return saveExtensionConfiguration(jsonCompact(document), error);
}
} // namespace

PluginManager::PluginManager(wl_event_loop *eventLoop) : eventLoop_(eventLoop) {
  std::optional<std::string> bundled = readFile(LUDASH_PLUGIN_CATALOG_SOURCE_FILE);
  if (!bundled)
    bundled = readFile(LUDASH_PLUGIN_CATALOG_INSTALL_FILE);
  if (!bundled) {
    storeError_ = "Could not read the bundled plugin catalogue.";
  } else {
    std::string error;
    if (!applyStoreCatalog(*bundled, &error))
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
  if (raw->source && wl_event_source_timer_update(raw->source, 1) >= 0)
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
  callback->source = nullptr;
  auto found = std::find_if(
      deferredCallbacks_.begin(), deferredCallbacks_.end(),
      [callback](const auto &entry) { return entry.get() == callback; });
  if (found == deferredCallbacks_.end())
    return;
  auto invoke = std::move(callback->callback);
  deferredCallbacks_.erase(found);
  invoke();
}

bool PluginManager::applyStoreCatalog(const std::string &bytes,
                                      std::string *error) {
  const auto root = parseJson(bytes);
  if (!root.is_object()) {
    if (error)
      *error = "Invalid plugin catalogue JSON";
    return false;
  }
  const auto values = jsonValue(root, "plugins");
  if (jsonInt(root, "schemaVersion") != 1 ||
      (jsonContains(root, "format") &&
       jsonString(root, "format") != "lunadash-plugin-index") ||
      !values.is_array() || values.size() > 256) {
    if (error)
      *error = "Unsupported or oversized plugin catalogue";
    return false;
  }

  Json normalized = Json::array();
  std::set<std::string> ids;
  for (const auto &value : values) {
    if (!value.is_object()) {
      if (error)
        *error = "Plugin catalogue entries must be objects";
      return false;
    }
    auto item = value;
    const auto id = jsonString(item, "id");
    const auto name = jsonString(item, "name");
    const auto version = jsonString(item, "version");
    bool validTargets = false;
    if (jsonValue(item, "targets").is_array()) {
      const auto targetItems = jsonArray(item, "targets");
      std::set<std::string> targetIds;
      validTargets = !targetItems.empty() && targetItems.size() <= 16;
      for (const auto &implementation : targetItems) {
        const auto targetId = jsonString(implementation, "target",
                                         jsonString(implementation, "id"));
        const auto type = jsonString(implementation, "type");
        const auto target = extensionTarget(targetId);
        if (!implementation.is_object() || targetId.empty() ||
            targetIds.contains(targetId) || target.empty() ||
            !arrayContains(jsonArray(target, "types"), Json(type))) {
          validTargets = false;
          break;
        }
        targetIds.insert(targetId);
      }
    } else {
      const auto type = jsonString(item, "type");
      const auto target = extensionTarget(jsonString(item, "target"));
      validTargets = !target.empty() &&
                     arrayContains(jsonArray(target, "types"), Json(type));
    }
    if (!validPluginId(id) || ids.contains(id) || name.empty() ||
        version.empty() || !validTargets) {
      if (error)
        *error = "Plugin catalogue contains an invalid identity or target";
      return false;
    }
    ids.insert(id);

    const auto *tags = jsonMember(item, "tags");
    if (!tags)
      item["tags"] = Json::array();
    else if (!tags->is_array() || tags->size() > 12 ||
             std::any_of(tags->begin(), tags->end(), [](const Json &tag) {
               if (!tag.is_string())
                 return true;
               const auto &text = tag.get_ref<const std::string &>();
               return trimmed(text) != text || text.empty() || text.size() > 32;
             })) {
      if (error)
        *error = "Plugin catalogue contains invalid tags";
      return false;
    }

    const auto icon = jsonString(item, "icon", "applications-system");
    if (icon.find('/') != std::string::npos && !isHttpsUrl(icon)) {
      if (error)
        *error = "Remote plugin icons must use HTTPS";
      return false;
    }
    item["icon"] = icon;

    if (jsonContains(item, "sourceUrl") &&
        !isHttpsUrl(jsonString(item, "sourceUrl"))) {
      if (error)
        *error = "Plugin source URLs must use HTTPS";
      return false;
    }

    if (const auto *install = jsonMember(item, "install")) {
      if (!install->is_object()) {
        if (error)
          *error = "Plugin install payload must be an object";
        return false;
      }
      const auto files = jsonValue(*install, "files");
      if (!files.is_array() || files.empty() || files.size() > 32) {
        if (error)
          *error = "Plugin install file list is invalid";
        return false;
      }
      std::set<std::string> paths;
      bool metadata = false;
      bool cmakeProject = false;
      for (const auto &file : files) {
        if (!file.is_object()) {
          if (error)
            *error = "Plugin install entries must be objects";
          return false;
        }
        const auto path = jsonString(file, "path");
        const auto url = jsonString(file, "url");
        const auto sha = jsonString(file, "sha256");
        if (!safeInstallPath(path) || paths.contains(path) ||
            !isHttpsUrl(url) || !validSha256(sha)) {
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
    normalized.push_back(item);
  }
  storeCatalog_ = normalized;
  if (error)
    error->clear();
  return true;
}

void PluginManager::refreshStore() {
  const char *environment = std::getenv("LUNADASH_PLUGIN_CATALOG_URL");
  const std::string configured =
      environment ? environment
                  : "https://raw.githubusercontent.com/LuYishan-4/"
                    "LunaDash-Plugins/main/index.json";
  if (configured.empty() || equalsIgnoringCase(configured, "off"))
    return;

  if (!isHttpsUrl(configured)) {
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
      "--show-error", "--", configured};
  const bool started = catalogRunner_->runDetailed(
      "curl", arguments,
      [this](const ProcessResult &result) {
        if (!defer([this, result] {
              storeLoading_ = false;
              std::string error;
              if (!result.success) {
                storeError_ =
                    "Remote catalogue unavailable; showing bundled catalogue";
              } else if (!applyStoreCatalog(result.output, &error)) {
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

bool PluginManager::installFromStore(const std::string &id,
                                     std::string *error) {
  if (id.empty()) {
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

  Json item = Json::object();
  for (const auto &candidate : storeCatalog_) {
    if (jsonString(candidate, "id") == id) {
      item = candidate;
      break;
    }
  }
  if (item.empty()) {
    if (error)
      *error = "Plugin is not in the reviewed store catalogue";
    return false;
  }
  const auto install = jsonObject(item, "install");
  const auto files = jsonArray(install, "files");
  if (install.empty() || files.empty()) {
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
  if (root.empty() || !makeDirectories(root)) {
    if (error)
      *error = "Could not create the user plugin directory";
    return false;
  }
  const auto destination = std::filesystem::path(root) / id;
  if (pathExists(destination) && !installedPackageFound) {
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
  job->stage = std::make_unique<TemporaryDirectory>(
      std::filesystem::path(root) / (".install-" + id + "-XXXXXX"));
  if (!job->stage->isValid()) {
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

  const auto &entry = job->files[job->index];
  const auto relativePath = jsonString(entry, "path");
  const auto expectedSha = asciiLower(jsonString(entry, "sha256"));
  const auto url = jsonString(entry, "url");
  const auto path = job->stage->filePath(relativePath);
  if (!makeDirectories(std::filesystem::path(path).parent_path())) {
    finishStoreInstall(job, "Could not create the plugin staging path");
    return;
  }
  job->process = std::make_unique<ProcessRunner>(eventLoop_, false);
  const std::vector<std::string> arguments{
      "--fail", "--location", "--proto", "=https", "--proto-redir", "=https",
      "--max-redirs", "5", "--max-filesize", "4194304", "--silent",
      "--show-error", "--output", path, "--", url};
  const bool started = job->process->runDetailed(
      "curl", arguments,
      [this, job, expectedSha, path](const ProcessResult &result) {
        if (!defer([this, job, expectedSha, path, result] {
              if (!result.success) {
                finishStoreInstall(job, "Plugin download failed");
                return;
              }
              const auto data = readFile(path, 4 * 1024 * 1024);
              if (!data) {
                finishStoreInstall(job,
                                   "Plugin file exceeds the 4 MiB limit or is unreadable");
                return;
              }
              if (sha256Hex(*data) != expectedSha) {
                finishStoreInstall(job,
                                   "Plugin file hash verification failed");
                return;
              }
              std::error_code permissionError;
              std::filesystem::permissions(
                  path,
                  std::filesystem::perms::owner_read |
                      std::filesystem::perms::owner_write,
                  std::filesystem::perm_options::replace, permissionError);
              if (permissionError) {
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
  std::error_code fileError;
  const auto cmakeFile = job->stage->filePath("CMakeLists.txt");
  const auto metadataFile = job->stage->filePath("metadata.json");
  if (!std::filesystem::is_regular_file(cmakeFile, fileError) ||
      !std::filesystem::is_regular_file(metadataFile, fileError)) {
    finishStoreInstall(job, "Downloaded plugin is missing its SDK CMake project");
    return;
  }

  const auto findExecutable = [](const char *name) {
    gchar *path = g_find_program_in_path(name);
    const std::string result = path ? std::string(path) : std::string();
    g_free(path);
    return result;
  };
  job->cmakeExecutable = findExecutable("cmake");
  const auto ninja = findExecutable("ninja");
  if (job->cmakeExecutable.empty() || ninja.empty()) {
    finishStoreInstall(
        job, "One-click plugin installation requires cmake and ninja");
    return;
  }

  job->buildDirectory = job->stage->filePath(".build");
  job->installPrefix = job->stage->filePath(".prefix");
  if (!makeDirectories(job->buildDirectory) ||
      !makeDirectories(job->installPrefix)) {
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

  job->process = std::make_unique<ProcessRunner>(eventLoop_, false);
  notifyChanged();
  const bool started = job->process->runDetailed(
      job->cmakeExecutable, arguments,
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

  const auto builtDirectory = std::filesystem::path(job->installPrefix) /
                              "share/lunadash/plugins" / job->id;
  const auto metadataPath = (builtDirectory / "metadata.json").string();
  const auto descriptors = readPluginMetadataTargets(metadataPath, false);
  if (descriptors.empty() ||
      std::any_of(descriptors.cbegin(), descriptors.cend(),
                  [&](const auto &descriptor) {
                    return descriptor.id != job->id ||
                           !descriptor.error.empty();
                  })) {
    finishStoreInstall(
        job, "Built plugin failed LunaDash SDK/runtime validation");
    return;
  }

  if (!writeFileAtomically(
          builtDirectory / ".lunadash-store.json",
          jsonCompact(Json{{"fingerprint", pluginStoreFingerprint(job->item)}}))) {
    finishStoreInstall(job, "Could not write the Store revision receipt");
    return;
  }

  const auto root = std::filesystem::path(userPluginRoot());
  const auto destination = root / job->id;
  const auto backup = root / (".old-" + job->id + "-" + millisecondsSinceEpoch());

  std::string configError;
  if (!removeExtensionPackageConfiguration(job->id, &configError)) {
    finishStoreInstall(
        job, configError.empty() ? "Could not reset the plugin configuration"
                                 : configError);
    return;
  }
  refresh();

  std::error_code renameError;
  bool backedUp = false;
  if (pathExists(destination)) {
    std::filesystem::rename(destination, backup, renameError);
    if (renameError) {
      finishStoreInstall(job, "Could not stage the existing plugin for update");
      return;
    }
    backedUp = true;
  }

  // The staging directory lives inside the plugin root, so this rename stays
  // on one filesystem and replaces the package atomically.
  std::filesystem::rename(builtDirectory, destination, renameError);
  if (renameError) {
    if (backedUp) {
      std::error_code restoreError;
      std::filesystem::rename(backup, destination, restoreError);
    }
    finishStoreInstall(job, "Could not install the built plugin package");
    return;
  }

  if (backedUp) {
    std::error_code removeError;
    std::filesystem::remove_all(backup, removeError);
  }
  finishStoreInstall(job, {});
}

void PluginManager::finishStoreInstall(StoreInstall *job,
                                       const std::string &error) {
  if (!job)
    return;
  const auto id = job->id;
  std::erase_if(storeInstalls_,
                [job](const auto &entry) { return entry.get() == job; });
  if (!error.empty()) {
    storeInstallErrors_[id] = error.substr(0, 1024);
    notifyChanged();
    return;
  }
  storeInstallErrors_.erase(id);
  refresh();
}

bool PluginManager::removeInstalledPlugin(const std::string &id,
                                          std::string *error) {
  if (!validPluginId(id)) {
    if (error)
      *error = "Invalid plugin id";
    return false;
  }

  const auto root = std::filesystem::path(userPluginRoot());
  const auto rootCanonical = canonicalPath(root);
  const auto destinationCanonical = canonicalPath(root / id);
  if (rootCanonical.empty() || destinationCanonical.empty() ||
      std::filesystem::path(destinationCanonical).parent_path().string() !=
          rootCanonical) {
    if (error)
      *error = "Only user-installed plugins can be removed";
    return false;
  }

  const auto metadataPath =
      (std::filesystem::path(destinationCanonical) / "metadata.json").string();
  const auto descriptors = readPluginMetadataTargets(metadataPath, false);
  if (descriptors.empty() ||
      std::none_of(descriptors.cbegin(), descriptors.cend(),
                   [&](const auto &descriptor) { return descriptor.id == id; })) {
    if (error)
      *error = "Installed plugin metadata does not match the requested id";
    return false;
  }

  std::string configError;
  if (!removeExtensionPackageConfiguration(id, &configError)) {
    if (error)
      *error = configError.empty() ? "Could not disable the plugin before removal"
                                   : configError;
    return false;
  }

  // Refresh first so native modules from this package are unloaded/staged
  // independently before any user files are removed.
  refresh();
  std::error_code removeError;
  std::filesystem::remove_all(destinationCanonical, removeError);
  if (removeError) {
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
  std::set<std::string> active;
  std::set<std::string> selectedTargets;

  for (auto descriptor : discovered) {
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

    if (descriptor.enabled && descriptor.error.empty() &&
        jsonString(extensionTarget(descriptor.target), "selection", "single") ==
            "single") {
      if (selectedTargets.contains(descriptor.target))
        descriptor.error =
            "Another plugin is already enabled for this single-owner target";
      else
        selectedTargets.insert(descriptor.target);
    }

    if (!descriptor.enabled || !descriptor.error.empty()) {
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
        native->stage = std::make_unique<TemporaryDirectory>(
            std::filesystem::path(g_get_tmp_dir()) / "lunadash-plugin-XXXXXX");
        const auto stagedLibrary = native->stage->filePath("plugin.so");
        std::error_code copyError;
        if (!native->stage->isValid()) {
          errors_[key] = "Could not create native plugin staging directory";
        } else if (!std::filesystem::copy_file(descriptor.libraryPath,
                                               stagedLibrary, copyError) ||
                   copyError) {
          errors_[key] = copyError ? copyError.message()
                                   : "Could not stage the plugin library";
        } else {
          native->library =
              ::dlopen(stagedLibrary.c_str(), RTLD_NOW | RTLD_LOCAL);
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
                               ? std::string(loadError)
                               : libraryError;
          } else {
            native->api = entry();
            const auto *api = native->api;
            if (!api || api->struct_size != sizeof(ludash_plugin_api) ||
                api->api_version != LUDASH_PLUGIN_API_VERSION || !api->process ||
                !api->metadata_json ||
                parseJson(api->metadata_json) != descriptor.manifest) {
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

Json PluginManager::snapshot() {
  if (!snapshotDirty_)
    return snapshotCache_;

  Json installed = Json::array();
  std::set<std::string> replacements;
  std::map<std::string, Json> storeItems;
  for (const auto &item : storeCatalog_)
    storeItems[jsonString(item, "id")] = item;
  const auto removableRoot = canonicalPath(userPluginRoot());
  static const std::set<std::string> retiredBundledIds{
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
    const Json storeItem =
        storeItemIt == storeItems.end() ? Json::object() : storeItemIt->second;
    const auto storeVersion = jsonString(storeItem, "version");
    item["outdated"] =
        !storeVersion.empty() && !pluginMatchesStore(descriptor, storeItem);
    item["storeVersion"] = storeVersion;
    const auto packageDir = canonicalPath(
        std::filesystem::absolute(descriptor.metadataPath).parent_path());
    item["removable"] =
        !removableRoot.empty() && !packageDir.empty() &&
        std::filesystem::path(packageDir).parent_path().string() == removableRoot;
    std::string error = descriptor.error;
    if (error.empty()) {
      const auto errorIt = errors_.find(pluginInstanceId(descriptor));
      if (errorIt != errors_.end())
        error = errorIt->second;
    }
    if (descriptor.enabled && error.empty() && descriptor.mode == "replace") {
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
    item["available"] = descriptor.enabled && loaded && error.empty();
    item["status"] = !error.empty()        ? "error"
                     : !descriptor.enabled ? "disabled"
                     : !loaded             ? "unavailable"
                                           : "available";
    installed.push_back(item);
  }
  std::string configError;
  const auto document = readExtensionConfiguration(&configError);
  auto targets = extensionTargets();
  for (auto &target : targets) {
    if (!target.is_object())
      target = Json::object();
    target["builtinSettings"] =
        configuredBuiltinSettings(jsonString(target, "id"), document);
  }
  Json remote = Json::array();
  for (auto item : storeCatalog_) {
    const auto id = jsonString(item, "id");
    std::string installedVersion;
    bool current = false;
    for (const auto &descriptor : catalog_) {
      if (descriptor.id == id && descriptor.schemaVersion == 2) {
        installedVersion = descriptor.version;
        current = pluginMatchesStore(descriptor, item);
        break;
      }
    }
    const bool isInstalled = !installedVersion.empty();
    item["installed"] = current;
    item["installedVersion"] = installedVersion;
    item["updateAvailable"] = isInstalled && !current;
    item["installable"] = jsonValue(item, "install").is_object();
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
    item["installError"] = installError == storeInstallErrors_.end()
                               ? std::string()
                               : installError->second;
    remote.push_back(item);
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

bool PluginManager::setEnabled(const std::string &id, bool enabled,
                               std::string *error) {
  auto document = readExtensionConfiguration();
  auto configs = jsonObject(document, "plugins");
  const auto discovered = discoverPlugins();
  for (const auto &plugin : discovered) {
    if (plugin.id != id)
      continue;
    auto package = jsonObject(configs, id);
    package["enabled"] = enabled;
    auto targets = jsonObject(package, "targets");
    for (const auto &candidate : discovered) {
      if (candidate.id != id)
        continue;
      targets[candidate.target] = Json{{"enabled", enabled},
                                       {"mode", candidate.mode},
                                       {"settings", candidate.settings}};
    }
    package.erase("mode");
    package.erase("settings");
    package["targets"] = targets;
    configs[id] = package;
    document["plugins"] = configs;
    return saveExtensionConfiguration(jsonCompact(document), error);
  }
  if (error)
    *error = "Unknown plugin";
  return false;
}

void PluginManager::reportError(const std::string &key,
                                const std::string &error) {
  if (!error.empty()) {
    errors_[key] = error.substr(0, 1024);
    notifyChanged();
    return;
  }

  errors_.erase(key);
  if (!inHook_)
    refresh();
}

std::string PluginManager::windowTemplateKey() const {
  for (const auto &native : native_) {
    const auto &plugin = native->current;
    if (plugin.enabled && plugin.error.empty() &&
        !errors_.contains(pluginInstanceId(plugin)) &&
        plugin.manifest == native->descriptor.manifest &&
        plugin.target == "window-layout" && plugin.mode == "replace")
      return jsonString(plugin.implementation, "windowTemplate", "tiling");
  }
  return "tiling";
}

Json PluginManager::filter(const std::string &target, const Json &builtin,
                           const Json &context, const Validator &validate) {
  HookStateGuard guard(inHook_);
  Json current = builtin;
  bool replaced = false;
  for (const std::string mode : {"replace", "augment"}) {
    for (const auto &native : native_) {
      const auto &descriptor = native->current;
      if (descriptor.target != target || descriptor.mode != mode ||
          !descriptor.enabled || !descriptor.error.empty() ||
          descriptor.manifest != native->descriptor.manifest ||
          errors_.contains(pluginInstanceId(descriptor)) ||
          (mode == "replace" && replaced))
        continue;
      const Json request{
          {"target", target},
          {"mode", mode},
          {"context", context.is_object() ? context : Json::object()},
          {"builtin", builtin},
          {"current", mode == "replace" ? Json::object() : current},
          {"settings", descriptor.settings}};
      std::string response(262145, '\0');
      int length = -1;
      try {
        const auto payload = jsonCompact(request);
        length = native->api->process(payload.c_str(), response.data(),
                                      response.size());
      } catch (...) {
        reportError(pluginInstanceId(descriptor), "Plugin threw across its C ABI");
        continue;
      }
      if (length < 0 || static_cast<std::size_t>(length) >= response.size()) {
        reportError(pluginInstanceId(descriptor),
                    "Plugin returned an invalid response length");
        continue;
      }
      const auto result =
          parseJson(std::string_view(response.data(),
                                     static_cast<std::size_t>(length)));
      if (!result.is_object() || !validate(result)) {
        reportError(pluginInstanceId(descriptor),
                    "Invalid plugin output; built-in/previous result retained");
        continue;
      }
      current = result;
      if (mode == "replace")
        replaced = true;
    }
  }
  return current;
}
} // namespace LunaDash
