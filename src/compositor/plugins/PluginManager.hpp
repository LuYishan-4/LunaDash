#pragma once
#include "config/plugins/PluginCatalog.hpp"
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>
class QQuickItem;
class QPluginLoader;
namespace LunaDash {
class CompositorPlugin;
class PluginManager final : public QObject {
public:
  explicit PluginManager(QObject *parent = nullptr);
  void loadEnabled();
  void windowOpened(QQuickItem *frame);
  void windowFocused(QQuickItem *frame);
  QStringList errors() const;
  QJsonObject snapshot() const;
  bool setEnabled(const QString &id, bool enabled, QString *error = nullptr);

private:
  QList<QPluginLoader *> loaders_;
  QList<CompositorPlugin *> plugins_;
  QStringList errors_;
};
} // namespace LunaDash
