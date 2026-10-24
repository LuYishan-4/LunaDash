#pragma once
#include "config/plugins/PluginCatalog.hpp"
#include "core/process/ProcessRunner.hpp"
#include <wayland-server-core.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace LunaDash {
class PluginManager final {
public:
  explicit PluginManager(wl_event_loop *eventLoop = nullptr);
  ~PluginManager();
  PluginManager(const PluginManager &) = delete;
  PluginManager &operator=(const PluginManager &) = delete;
  void loadEnabled();
  Json snapshot();
  void refresh();
  bool setEnabled(const std::string &id, bool enabled,
                  std::string *error = nullptr);
  bool installFromStore(const std::string &id, std::string *error = nullptr);
  bool removeInstalledPlugin(const std::string &id,
                             std::string *error = nullptr);
  using Validator = std::function<bool(const Json &)>;
  Json filter(const std::string &target, const Json &builtin,
              const Json &context, const Validator &validate);
  void reportError(const std::string &id, const std::string &error);
  std::string windowTemplateKey() const;
  void setChangedCallback(std::function<void()> callback);

private:
  struct Native;
  struct StoreInstall;
  struct DeferredCallback;
  std::vector<std::unique_ptr<Native>> native_;
  std::vector<std::unique_ptr<StoreInstall>> storeInstalls_;
  std::map<std::string, std::string> errors_;
  std::map<std::string, std::string> storeInstallErrors_;
  std::vector<PluginDescriptor> catalog_;
  Json storeCatalog_ = Json::array();
  std::string storeError_;
  wl_event_loop *eventLoop_ = nullptr;
  std::unique_ptr<ProcessRunner> catalogRunner_;
  bool storeLoading_ = false;
  bool inHook_ = false;
  bool snapshotDirty_ = true;
  Json snapshotCache_ = Json::object();
  std::function<void()> changedCallback_;
  std::vector<std::unique_ptr<DeferredCallback>> deferredCallbacks_;

  void notifyChanged();
  bool defer(std::function<void()> callback);
  static int handleDeferredTimer(void *data);
  void dispatchDeferred(DeferredCallback *callback);
  void refreshStore();
  bool applyStoreCatalog(const std::string &bytes, std::string *error);
  void continueStoreInstall(StoreInstall *job);
  void startStoreBuild(StoreInstall *job);
  void runStoreBuildStep(StoreInstall *job);
  void installBuiltStorePackage(StoreInstall *job);
  void finishStoreInstall(StoreInstall *job, const std::string &error);
};
} // namespace LunaDash
