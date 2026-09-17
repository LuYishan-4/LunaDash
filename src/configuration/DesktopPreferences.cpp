#include <LuDash/configuration/DesktopPreferences.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <cmath>

namespace LuDash {
namespace {
QJsonObject defaults() {
    return {{"accent", "#9ccbfb"}, {"gap", 12}, {"panelHeight", 40},
            {"blur", true}, {"blurRadius", 18}, {"windowOpacity", 96}, {"animations", true}, {"animationDuration", 220},
            {"workspaceCount", 4}, {"masterRatio", 56}, {"defaultFloating", false}, {"altMouseResize", true},
            {"keyboardLayout", "us"}, {"keyRepeatRate", 25}, {"keyRepeatDelay", 600}, {"cursorSize", 24},
            {"fontFamily", "sans-serif"}, {"clock24Hour", true}, {"startupApps", QJsonArray{}},
            {"proxyEnabled", false}, {"proxyHttp", ""}, {"proxyHttps", ""}, {"proxySocks", ""}, {"proxyBypass", ""},
            {"overview", false}, {"showHostDetails", false}, {"updateChannel", "stable"}};
}

QString localized(const QJsonObject &object, const QString &key,
                  const QString &locale) {
    return object.value(key + "[" + locale + "]")
        .toString(object.value(key).toString());
}

QJsonArray pluginSnapshot() {
    QStringList roots;
    roots << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../qml/plugins");
    for (const auto &path : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        roots << path + "/lunadash/shell/plugins";
        roots << path + "/ludash/plugins";
    }
    roots << QCoreApplication::applicationDirPath() + "/plugins";

    const QString locale = QSettings().value("appearance/language", "en_US").toString();
    QSet<QString> seen;
    QJsonArray result;
    for (const auto &root : roots) {
        const QDir directory(root);
        for (const auto &folder : directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile file(directory.filePath(folder + "/metadata.json"));
            if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
                continue;
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
                continue;
            const auto metadata = document.object();
            QString id;
            QString name;
            QString description;
            QString version;
            QString author;
            QString icon = "applications-system";
            QString type;
            QString entry;
            bool enabledByDefault = false;
            if (metadata.contains("KPlugin")) {
                const auto info = metadata.value("KPlugin").toObject();
                const auto api = metadata.value("LuDash").toObject();
                id = info.value("Id").toString();
                name = localized(info, "Name", locale);
                description = localized(info, "Description", locale);
                version = info.value("Version").toString();
                icon = info.value("Icon").toString(icon);
                if (!info.value("Authors").toArray().isEmpty())
                    author = info.value("Authors").toArray().first().toObject().value("Name").toString();
                type = api.value("Type").toString() == "WindowEffect" ? "effect" : "unknown";
            } else if (metadata.value("schemaVersion").toInt() == 1) {
                id = metadata.value("id").toString();
                name = localized(metadata, "name", locale);
                description = localized(metadata, "description", locale);
                version = metadata.value("version").toString();
                icon = metadata.value("icon").toString(icon);
                type = metadata.value("type").toString().toLower();
                enabledByDefault = metadata.value("enabledByDefault").toBool(false);
                const auto authorValue = metadata.value("author");
                author = authorValue.isObject() ? authorValue.toObject().value("name").toString()
                                                : authorValue.toString();
                if (type == "qml") {
                    const QString candidate = QFileInfo(file).absoluteDir().filePath(metadata.value("entry").toString());
                    const QString canonicalRoot = QFileInfo(QFileInfo(file).absolutePath()).canonicalFilePath();
                    const QString canonicalEntry = QFileInfo(candidate).canonicalFilePath();
                    if (!canonicalEntry.isEmpty() && QFileInfo(canonicalEntry).absolutePath() == canonicalRoot && canonicalEntry.endsWith(".qml"))
                        entry = QUrl::fromLocalFile(canonicalEntry).toString();
                }
            }
            if (id.isEmpty() || seen.contains(id) || name.isEmpty() || version.isEmpty() ||
                (type != "qml" && type != "effect"))
                continue;
            if (type == "qml" && entry.isEmpty())
                continue;
            seen.insert(id);
            const bool enabled = QSettings().value("plugins/" + id + "/enabled", enabledByDefault).toBool();
            result.append(QJsonObject{{"id", id}, {"name", name}, {"description", description},
                                      {"version", version}, {"author", author}, {"icon", icon},
                                      {"type", type}, {"entry", entry}, {"enabled", enabled},
                                      {"restartRequired", type == "effect"}});
        }
    }
    return result;
}

bool validProxyText(const QJsonValue &value, bool url) {
    if (!value.isString()) return false;
    const QString text = value.toString();
    if (text.size() > 512 || text.contains('\n') || text.contains('\r') || text.contains(QChar::Null)) return false;
    if (!url || text.isEmpty()) return true;
    const QUrl parsed(text);
    return parsed.isValid() && QStringList{"http", "https", "socks", "socks5"}.contains(parsed.scheme().toLower());
}

bool valid(const QString& key, const QJsonValue& value) {
    if (key == "keyboardLayout") return value.isString() && QStringList{"us", "gb", "de", "fr", "es", "jp", "tw"}.contains(value.toString());
    if (key == "fontFamily") return value.isString() && QStringList{"sans-serif", "serif", "monospace"}.contains(value.toString());
    if (key == "updateChannel") return value.isString() && QStringList{"stable", "dev"}.contains(value.toString());
    if (key == "proxyHttp" || key == "proxyHttps" || key == "proxySocks") return validProxyText(value, true);
    if (key == "proxyBypass") return validProxyText(value, false);
    if (key == "startupApps") {
        if (!value.isArray() || value.toArray().size() > 4) return false;
        QSet<QString> seen;
        for (const auto& app : value.toArray()) {
            if (!app.isString() || !QStringList{"files", "console", "monitor", "welcome"}.contains(app.toString()) || seen.contains(app.toString())) return false;
            seen.insert(app.toString());
        }
        return true;
    }
    const QMap<QString, QPair<int, int>> ranges{{"workspaceCount", {1, 9}}, {"masterRatio", {30, 70}}, {"keyRepeatRate", {0, 60}}, {"keyRepeatDelay", {200, 1500}}, {"cursorSize", {16, 64}}};
    if (ranges.contains(key)) {
        const double number = value.toDouble(-1); const auto range = ranges.value(key);
        return value.isDouble() && std::isfinite(number) && std::floor(number) == number && number >= range.first && number <= range.second;
    }
    if (key == "accent") return value.isString() && QRegularExpression("^#[0-9a-fA-F]{6}$").match(value.toString()).hasMatch();
    if (key == "overview" || key == "showHostDetails" || key == "blur" || key == "animations" || key == "defaultFloating" || key == "altMouseResize" || key == "clock24Hour" || key == "proxyEnabled") return value.isBool();
    if (key == "gap" || key == "panelHeight" || key == "blurRadius" || key == "windowOpacity" || key == "animationDuration") {
        const double number = value.toDouble(-1);
        return value.isDouble() && std::isfinite(number) && std::floor(number) == number &&
               number >= (key == "gap" ? 4 : key == "panelHeight" ? 32 : key == "windowOpacity" ? 60 : 0) &&
               number <= (key == "gap" || key == "blurRadius" ? 32 : key == "panelHeight" ? 56 : key == "windowOpacity" ? 100 : 600);
    }
    return false;
}
}

QJsonObject desktopPreferences() {
    auto result = defaults();
    QSettings settings;
    for (auto it = result.begin(); it != result.end(); ++it) {
        auto value = QJsonValue::fromVariant(settings.value("desktop/" + it.key(), it.value().toVariant()));
        if (value.isString() && it.value().isBool()) {
            if (value.toString() == "true") value = true;
            else if (value.toString() == "false") value = false;
        } else if (value.isString() && it.value().isDouble()) {
            bool ok = false;
            const double number = value.toString().toDouble(&ok);
            if (ok) value = number;
        }
        if (valid(it.key(), value)) it.value() = value;
    }
    result.insert("plugins", pluginSnapshot());
    return result;
}

bool updateDesktopPreferences(const QJsonObject& changes, QString* error) {
    for (auto it = changes.begin(); it != changes.end(); ++it) {
        if (!valid(it.key(), it.value())) {
            if (error) *error = "Invalid desktop preference: " + it.key();
            return false;
        }
    }
    QSettings settings;
    for (auto it = changes.begin(); it != changes.end(); ++it) settings.setValue("desktop/" + it.key(), it.value().toVariant());
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) *error = "Could not save desktop preferences.";
        return false;
    }
    return true;
}
bool setupComplete() { return qEnvironmentVariableIntValue("LUDASH_SKIP_SETUP") == 1 || QSettings().value("session/setupComplete", false).toBool(); }
void setSetupComplete(bool complete) { QSettings settings; settings.setValue("session/setupComplete", complete); settings.sync(); }
}
