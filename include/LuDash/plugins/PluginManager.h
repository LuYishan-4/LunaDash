#pragma once
#include <QObject>
#include <QStringList>
#include <QList>
#include <QJsonObject>
class QQuickItem;
class QPluginLoader;
namespace LuDash {
class CompositorPlugin;
struct PluginDescriptor {
    QString id;
    QString name;
    QString description;
    QString version;
    QString libraryPath;
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
private:
    QList<QPluginLoader*> loaders_;
    QList<CompositorPlugin*> plugins_;
    QStringList errors_;
};
}
