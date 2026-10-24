#pragma once
#include "config/plugins/PluginCatalog.hpp"
#include "core/process/ProcessRunner.hpp"
#include <QJsonArray>
#include <wayland-server-core.h>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace LunaDash {
class PluginManager final {
public:
  explicit PluginManager(wl_event_loop *eventLoop = nullptr);
  ~PluginManager();
  PluginManager(const PluginManager &) = delete;
  PluginManager &operator=(const PluginManager &) = delete;
  void loadEnabled();
  QJsonObject snapshot();
  void refresh();
  bool setEnabled(const QString &id, bool enabled, QString *error = nullptr);
  bool installFromStore(const QString &id, QString *error = nullptr);
  bool removeInstalledPlugin(const QString &id, QString *error = nullptr);
  using Validator = std::function<bool(const QJsonObject &)>;
  QJsonObject filter(const QString &target, const QJsonObject &builtin,
                     const QJsonObject &context, const Validator &validate);
  void reportError(const QString &id, const QString &error);
  QString windowTemplateKey() const;
  void setChangedCallback(std::function<void()> callback);

private:
  struct Native;
  struct StoreInstall;
  struct DeferredCallback;
  std::vector<std::unique_ptr<Native>> native_;
  std::vector<std::unique_ptr<StoreInstall>> storeInstalls_;
  std::map<QString, QString> errors_;
  std::map<QString, QString> storeInstallErrors_;
  std::vector<PluginDescriptor> catalog_;
  QJsonArray storeCatalog_;
  QString storeError_;
  wl_event_loop *eventLoop_ = nullptr;
  std::unique_ptr<ProcessRunner> catalogRunner_;
  bool storeLoading_ = false;
  bool inHook_ = false;
  bool snapshotDirty_ = true;
  QJsonObject snapshotCache_;
  std::function<void()> changedCallback_;
  std::vector<std::unique_ptr<DeferredCallback>> deferredCallbacks_;

  void notifyChanged();
  bool defer(std::function<void()> callback);
  void dispatchDeferred(DeferredCallback *callback);
  void refreshStore();
  bool applyStoreCatalog(const QByteArray &bytes, QString *error);
  void continueStoreInstall(StoreInstall *job);
  void startStoreBuild(StoreInstall *job);
  void runStoreBuildStep(StoreInstall *job);
  void installBuiltStorePackage(StoreInstall *job);
  void finishStoreInstall(StoreInstall *job, const QString &error);
};
} // namespace LunaDash
