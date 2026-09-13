#include <LuDash/plugins/PluginManager.h>
#include <LuDash/plugins/CompositorPlugin.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocale>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSet>
namespace LuDash {
PluginDescriptor readPluginMetadata(const QString& path) {
    PluginDescriptor result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) { result.error = "Unreadable or oversized metadata"; return result; }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) { result.error = "Invalid JSON metadata"; return result; }
    const auto metadata = document.object(); const auto info = metadata.value("KPlugin").toObject(); const auto api = metadata.value("LuDash").toObject();
    result.id = info.value("Id").toString(); result.version = info.value("Version").toString();
    const auto locale = QSettings().value("appearance/language", QLocale::system().name()).toString();
    result.name = info.value("Name[" + locale + "]").toString(info.value("Name").toString());
    result.description = info.value("Description[" + locale + "]").toString(info.value("Description").toString());
    static const QRegularExpression validId("^[a-zA-Z0-9][a-zA-Z0-9._-]+$");
    if (!validId.match(result.id).hasMatch() || result.name.isEmpty() || result.version.isEmpty()) { result.error = "Missing or invalid plugin identity"; return result; }
    if (api.value("ApiVersion").toInt() != 1 || api.value("Type").toString() != "WindowEffect") { result.error = "Unsupported LuDash plugin API or type"; return result; }
    const auto library = api.value("Library").toString();
    if (library.isEmpty() || library.contains('/') || library.contains('\\') || library == "." || library == "..") { result.error = "Plugin library must be a filename inside its directory"; return result; }
    const QDir directory = QFileInfo(file).absoluteDir();
    const auto canonicalDirectory = QFileInfo(directory.absolutePath()).canonicalFilePath();
    result.libraryPath = QFileInfo(directory.filePath(library)).canonicalFilePath();
    if (result.libraryPath.isEmpty() || QFileInfo(result.libraryPath).absolutePath() != canonicalDirectory) { result.error = "Missing library or library path escapes plugin directory"; return result; }
    result.enabled = QSettings().value("plugins/" + result.id + "/enabled", false).toBool();
    return result;
}
QList<PluginDescriptor> discoverPlugins() {
    QStringList roots;
    for (const auto& path : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) roots << path + "/ludash/plugins";
    roots << QCoreApplication::applicationDirPath() + "/plugins";
    QList<PluginDescriptor> result; QSet<QString> seen;
    for (const auto& root : roots) {
        const QDir directory(root);
        for (const auto& name : directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            auto plugin = readPluginMetadata(directory.filePath(name + "/metadata.json"));
            if (seen.contains(plugin.id) && !plugin.id.isEmpty()) continue;
            if (!plugin.id.isEmpty()) seen.insert(plugin.id);
            result << plugin;
        }
    }
    return result;
}
PluginManager::PluginManager(QObject* parent) : QObject(parent) {}
void PluginManager::loadEnabled() {
    for (const auto& descriptor : discoverPlugins()) {
        if (!descriptor.error.isEmpty()) { errors_ << descriptor.error; continue; }
        if (!descriptor.enabled) continue;
        auto* loader = new QPluginLoader(descriptor.libraryPath, this);
        const auto embedded = loader->metaData();
        if (embedded.value("IID").toString() != LUDASH_COMPOSITOR_PLUGIN_IID
            || embedded.value("MetaData").toObject().value("KPlugin").toObject().value("Id").toString() != descriptor.id) {
            errors_ << descriptor.id + ": metadata/IID mismatch"; delete loader; continue;
        }
        auto* plugin = qobject_cast<CompositorPlugin*>(loader->instance());
        if (!plugin) { errors_ << descriptor.id + ": " + loader->errorString(); delete loader; continue; }
        loaders_ << loader; plugins_ << plugin;
    }
}
void PluginManager::windowOpened(QQuickItem* frame) { for (auto* plugin : plugins_) plugin->windowOpened(frame); }
void PluginManager::windowFocused(QQuickItem* frame) { for (auto* plugin : plugins_) plugin->windowFocused(frame); }
QStringList PluginManager::errors() const { return errors_; }
}
