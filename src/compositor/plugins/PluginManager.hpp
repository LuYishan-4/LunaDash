#pragma once
#include "config/plugins/PluginCatalog.hpp"
#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <functional>
#include <memory>
#include <vector>
class QNetworkAccessManager;

namespace LunaDash {
class PluginManager final : public QObject {
  Q_OBJECT
public:
  explicit PluginManager(QObject *parent = nullptr);
  ~PluginManager() override;
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
signals:
  void changed();

private:
  struct Native;
  struct StoreInstall;
  std::vector<std::unique_ptr<Native>> native_;
  std::vector<std::unique_ptr<StoreInstall>> storeInstalls_;
  QHash<QString, QString> errors_;
  QHash<QString, QString> storeInstallErrors_;
  QList<PluginDescriptor> catalog_;
  QJsonArray storeCatalog_;
  QString storeError_;
  QNetworkAccessManager *storeNetwork_ = nullptr;
  bool storeLoading_ = false;
  bool inHook_ = false;
  bool snapshotDirty_ = true;
  QJsonObject snapshotCache_;

  void refreshStore();
  bool applyStoreCatalog(const QByteArray &bytes, QString *error);
  void continueStoreInstall(StoreInstall *job);
  void startStoreBuild(StoreInstall *job);
  void runStoreBuildStep(StoreInstall *job);
  void installBuiltStorePackage(StoreInstall *job);
  void finishStoreInstall(StoreInstall *job, const QString &error);
};
} // namespace LunaDash
