// Include GLib before Qt: GLib has fields named "signals".
#include <gio/gdesktopappinfo.h>
#include <LuDash/file_associations/FileAssociations.h>
#include <LuDash/localization/Localization.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>
#include <utility>

namespace LuDash {
namespace {
bool fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}
bool validId(const QString& id) {
    if (id.size() > 255 || !id.endsWith(".desktop")) return false;
    for (const auto ch : id)
        if (ch.isSpace() || ch.unicode() < 32 || ch == '/' || ch == '\\') return false;
    return true;
}
bool validMime(const QString& mime) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9!#$&^_.+-]+/[A-Za-z0-9!#$&^_.+-]+$"));
    return mime.size() <= 255 && pattern.match(mime).hasMatch();
}
bool usable(GAppInfo* app) {
    if (!app) return false;
    const QString executable = QFileInfo(QString::fromUtf8(g_app_info_get_executable(app))).fileName();
    return executable != "lunadash-desktop" && executable != "ludash-desktop" &&
           executable != "lunadashctl" && executable != "ludashctl" &&
           validId(QString::fromUtf8(g_app_info_get_id(app)));
}
GDesktopAppInfo* desktopApp(const QString& id) {
    if (!validId(id)) return nullptr;
    auto* app = g_desktop_app_info_new(id.toUtf8().constData());
    if (app && !usable(G_APP_INFO(app))) { g_object_unref(app); return nullptr; }
    return app;
}
QString iconName(GIcon* icon) {
    if (!icon) return {};
    if (G_IS_THEMED_ICON(icon)) {
        const auto names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names && names[0]) return QString::fromUtf8(names[0]);
    } else if (G_IS_FILE_ICON(icon)) {
        auto* path = g_file_get_path(g_file_icon_get_file(G_FILE_ICON(icon)));
        const QString result = QFile::decodeName(path ? path : "");
        g_free(path);
        return result;
    }
    return {};
}
QJsonObject defaults() {
    return {{"version", 1}, {"initialized", false}, {"askOnFirstOpen", true},
            {"associations", QJsonObject{}}};
}
bool validDocument(const QJsonObject& document) {
    const QSet<QString> keys{"version", "initialized", "askOnFirstOpen", "associations"};
    for (auto it = document.begin(); it != document.end(); ++it)
        if (!keys.contains(it.key())) return false;
    if (document.value("version").toDouble() != 1 ||
        !document.value("initialized").isBool() ||
        !document.value("askOnFirstOpen").isBool() ||
        !document.value("associations").isObject()) return false;
    const auto rules = document.value("associations").toObject();
    if (rules.size() > 4096) return false;
    for (auto it = rules.begin(); it != rules.end(); ++it) {
        const auto rule = it.value().toObject();
        const auto id = rule.value("desktopId").toString();
        if (!FileAssociations::validKey(it.key()) || !it.value().isObject() ||
            rule.size() != 2 || (id != "__system__" && !validId(id)) ||
            !validMime(rule.value("mimeType").toString())) return false;
    }
    return true;
}
} // namespace

FileAssociations::FileAssociations(QString path)
    : path_(path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                            "/LunaDash/file-associations.json" : std::move(path)) {}
QString FileAssociations::path() const { return path_; }
QJsonObject FileAssociations::read(QString* error) const {
    if (error) error->clear();
    QFile file(path_);
    if (!file.exists()) return defaults();
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
        fail(error, translate("Cannot read file associations. The existing configuration was not changed."));
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        !validDocument(document.object())) {
        fail(error, translate("Invalid file association configuration. The existing file was not overwritten."));
        return {};
    }
    return document.object();
}
QString FileAssociations::keyForExtension(const QString& input) {
    QString extension = input.trimmed().toLower();
    if (extension.startsWith("*.")) extension.remove(0, 2);
    else if (extension.startsWith('.')) extension.remove(0, 1);
    if (extension.isEmpty() || extension.size() > 64 || extension.startsWith('.') ||
        extension.endsWith('.') || extension.contains("..")) return {};
    for (const auto c : extension)
        if (c.isSpace() || c.category() == QChar::Other_Control ||
            QStringLiteral("/\\:*?<>|\"").contains(c)) return {};
    return "ext:" + extension;
}
bool FileAssociations::validKey(const QString& key) {
    if (key.startsWith("ext:")) return keyForExtension(key.mid(4)) == key;
    return key.startsWith("mime:") && validMime(key.mid(5));
}
QString FileAssociations::mimeTypeForFile(const QString& path) {
    return QMimeDatabase().mimeTypeForFile(path).name();
}
QString FileAssociations::keyForFile(const QString& path) const {
    const QFileInfo info(path);
    const QString name = info.fileName().toLower();
    const auto rules = read().value("associations").toObject();
    QString matched;
    for (auto it = rules.begin(); it != rules.end(); ++it)
        if (it.key().startsWith("ext:") && name.endsWith('.' + it.key().mid(4)) &&
            it.key().size() > matched.size()) matched = it.key();
    if (!matched.isEmpty()) return matched;
    auto suffix = QMimeDatabase().suffixForFileName(info.fileName());
    if (suffix.isEmpty()) suffix = info.suffix();
    if (name.startsWith('.') && name.count('.') == 1) suffix.clear();
    const auto key = keyForExtension(suffix);
    return key.isEmpty() ? "mime:" + mimeTypeForFile(path) : key;
}
QString FileAssociations::applicationForFile(const QString& path, QString* error) const {
    return read(error).value("associations").toObject().value(keyForFile(path))
        .toObject().value("desktopId").toString();
}
bool FileAssociations::update(const QString& key, const QJsonObject& value,
                              bool preferences, QString* error) const {
    if (error) error->clear();
    if (!QDir().mkpath(QFileInfo(path_).absolutePath()))
        return fail(error, translate("Could not save file associations."));
    QLockFile lock(path_ + ".lock");
    if (!lock.tryLock(100))
        return fail(error, translate("File associations are being edited in another window. Try again."));
    auto document = read(error);
    if (document.isEmpty()) return false;
    if (preferences) {
        document["initialized"] = value.value("initialized");
        document["askOnFirstOpen"] = value.value("askOnFirstOpen");
    } else {
        auto rules = document.value("associations").toObject();
        if (value.isEmpty()) rules.remove(key);
        else rules[key] = value;
        document["associations"] = rules;
    }
    if (!validDocument(document))
        return fail(error, translate("Invalid file association rule."));
    QSaveFile file(path_);
    const auto bytes = QJsonDocument(document).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, translate("Could not save file associations."));
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, translate("Could not save file associations."));
    return true;
}
bool FileAssociations::setPreferences(bool initialized, bool askOnFirstOpen, QString* error) const {
    return update({}, {{"initialized", initialized}, {"askOnFirstOpen", askOnFirstOpen}}, true, error);
}
bool FileAssociations::setRule(const QString& key, const QString& desktopId,
                               const QString& mimeType, QString* error) const {
    if (!validKey(key) || !validMime(mimeType))
        return fail(error, translate("Invalid file association rule."));
    if (!fileApplicationAvailable(desktopId))
        return fail(error, translate("The selected application is no longer installed."));
    return update(key, {{"desktopId", desktopId}, {"mimeType", mimeType}}, false, error);
}
bool FileAssociations::removeRule(const QString& key, QString* error) const {
    if (!validKey(key)) return fail(error, translate("Invalid file association rule."));
    return update(key, {}, false, error);
}
bool fileApplicationAvailable(const QString& desktopId) {
    if (desktopId == "__system__") return true;
    auto* app = desktopApp(desktopId);
    if (!app) return false;
    g_object_unref(app);
    return true;
}
QString systemFileApplication(const QString& mimeType) {
    auto* app = g_app_info_get_default_for_type(mimeType.toUtf8().constData(), false);
    const QString result = usable(app) ? QString::fromUtf8(g_app_info_get_id(app)) : QString{};
    if (app) g_object_unref(app);
    return result;
}
QList<FileApplication> fileApplications(const QString& mimeType) {
    QSet<QString> recommended;
    const auto type = mimeType.toUtf8();
    auto* matches = g_app_info_get_all_for_type(type.constData());
    for (auto* item = matches; item; item = item->next)
        recommended.insert(QString::fromUtf8(g_app_info_get_id(G_APP_INFO(item->data))));
    g_list_free_full(matches, g_object_unref);
    const auto defaultId = systemFileApplication(mimeType);
    QList<FileApplication> result;
    auto* all = g_app_info_get_all();
    QSet<QString> seen;
    for (auto* item = all; item; item = item->next) {
        auto* app = G_APP_INFO(item->data);
        if (!usable(app)) continue;
        const auto id = QString::fromUtf8(g_app_info_get_id(app));
        if (seen.contains(id) || (!g_app_info_should_show(app) && !recommended.contains(id) && id != defaultId)) continue;
        seen.insert(id);
        result.append({id, QString::fromUtf8(g_app_info_get_display_name(app)),
                       iconName(g_app_info_get_icon(app)), recommended.contains(id), id == defaultId});
    }
    g_list_free_full(all, g_object_unref);
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.systemDefault != right.systemDefault) return left.systemDefault;
        if (left.recommended != right.recommended) return left.recommended;
        return QString::localeAwareCompare(left.name, right.name) < 0;
    });
    return result;
}
bool setSystemFileApplication(const QString& desktopId, const QString& mimeType, QString* error) {
    if (error) error->clear();
    if (!validMime(mimeType)) return fail(error, translate("Invalid file association rule."));
    auto* app = desktopApp(desktopId);
    if (!app) return fail(error, translate("The selected application is no longer installed."));
    GError* nativeError = nullptr;
    const bool result = g_app_info_set_as_default_for_type(G_APP_INFO(app), mimeType.toUtf8().constData(), &nativeError);
    if (!result) fail(error, translate("Could not change the system default: %1").arg(
        nativeError ? QString::fromUtf8(nativeError->message) : QString{}));
    g_clear_error(&nativeError);
    g_object_unref(app);
    return result;
}
bool launchFilesWithApplication(const QString& desktopId, const QStringList& files, QString* error) {
    if (error) error->clear();
    if (files.isEmpty() || files.size() > 256)
        return fail(error, translate("Select between 1 and 256 items."));
    for (const auto& path : files)
        if (!QFileInfo(path).isFile()) return fail(error, translate("Source no longer exists: %1").arg(path));
    const auto resolved = desktopId == "__system__" ? systemFileApplication(FileAssociations::mimeTypeForFile(files.first())) : desktopId;
    auto* app = desktopApp(resolved);
    if (!app) return fail(error, translate("The selected application is no longer installed."));
    GList* list = nullptr;
    for (const auto& path : files)
        list = g_list_prepend(list, g_file_new_for_path(QFile::encodeName(QFileInfo(path).absoluteFilePath()).constData()));
    list = g_list_reverse(list);
    GError* nativeError = nullptr;
    const bool result = g_app_info_launch(G_APP_INFO(app), list, nullptr, &nativeError);
    if (!result) fail(error, translate("Could not open the file: %1").arg(
        nativeError ? QString::fromUtf8(nativeError->message) : QString{}));
    g_clear_error(&nativeError);
    g_list_free_full(list, g_object_unref);
    g_object_unref(app);
    return result;
}
} // namespace LuDash
