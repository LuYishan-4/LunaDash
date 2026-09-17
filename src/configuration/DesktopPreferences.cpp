#include <LuDash/configuration/DesktopPreferences.h>
#include <QRegularExpression>
#include <QSettings>
#include <cmath>
#include <QJsonArray>
#include <QMap>
#include <QStringList>
#include <QSet>

namespace LuDash {
namespace {
QJsonObject defaults() {
    return {{"accent", "#9ccbfb"}, {"gap", 12}, {"panelHeight", 40},
            {"blur", true}, {"blurRadius", 18}, {"windowOpacity", 96}, {"animations", true}, {"animationDuration", 220},
            {"workspaceCount", 4}, {"masterRatio", 56}, {"defaultFloating", false}, {"altMouseResize", true},
            {"keyboardLayout", "us"}, {"keyRepeatRate", 25}, {"keyRepeatDelay", 600}, {"cursorSize", 24},
            {"fontFamily", "sans-serif"}, {"clock24Hour", true}, {"startupApps", QJsonArray{}},
            {"overview", false}, {"showHostDetails", false}, {"updateChannel", "stable"}};
}
bool valid(const QString& key, const QJsonValue& value) {
    if (key == "keyboardLayout") return value.isString() && QStringList{"us", "gb", "de", "fr", "es", "jp", "tw"}.contains(value.toString());
    if (key == "fontFamily") return value.isString() && QStringList{"sans-serif", "serif", "monospace"}.contains(value.toString());
    if (key == "updateChannel") return value.isString() && QStringList{"stable", "dev"}.contains(value.toString());
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
    if (key == "overview" || key == "showHostDetails" || key == "blur" || key == "animations" || key == "defaultFloating" || key == "altMouseResize" || key == "clock24Hour") return value.isBool();
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
