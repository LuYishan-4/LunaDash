#pragma once
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>
class QQuickItem;
class QPluginLoader;
namespace LuDash {
class CompositorPlugin;
struct PluginDescriptor {
    QString id;
    QString name;
    QString description;
    QString version;
    QString author;
    QString icon;
    QString type;
    QString entryPath;
    QString libraryPath;
    QString metadataPath;
    QString error;
    bool enabled = false;
};
PluginDescriptor readPluginMetadata(const QString& metadataPath);
QList<PluginDescriptor> discoverPlugins();
class PluginManager final : public QObject {
public:
    explicit PluginManager(QObject* parent = nullptr);
    void loadEnabled();
    void windowOpened(QQuickItem* frame);
    void windowFocused(QQuickItem* frame);
    QStringList errors() const;
    QJsonObject snapshot() const;
    bool setEnabled(const QString& id, bool enabled, QString* error = nullptr);
private:
    QList<QPluginLoader*> loaders_;
    QList<CompositorPlugin*> plugins_;
    QStringList errors_;
};
}
