#pragma once
#include "config/plugins/PluginCatalog.hpp"
#include <QHash>
#include <QObject>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>
namespace LunaDash {
class PluginBundle;
class PluginManager final : public QObject {
  Q_OBJECT
public:
  explicit PluginManager(QObject *parent = nullptr);
  ~PluginManager() override;
  void loadEnabled();
  QJsonObject snapshot();
  void refresh();
  bool setEnabled(const QString &id, bool enabled, QString *error = nullptr);
  using Validator = std::function<bool(const QJsonObject &)>;
  QJsonObject filter(const QString &target, const QJsonObject &builtin,
                     const QJsonObject &context, const Validator &validate);
  void reportError(const QString &id, const QString &error);
  bool stackingLayout() const;
signals:
  void changed();

private:
  struct Native;
  std::vector<std::unique_ptr<Native>> native_;
  QHash<QString, QString> errors_;
  QHash<QString, QString> revisions_;
  QHash<QString, QString> attempts_;
  QHash<QString, std::shared_ptr<PluginBundle>> bundles_;
  QList<std::shared_ptr<PluginBundle>> retired_;
  QList<PluginDescriptor> catalog_;
  QTimer poll_;
  bool inHook_ = false;
};
} // namespace LunaDash
