#include <LuDash/configuration/DesktopPreferences.h>
#include <QRegularExpression>
#include <QSettings>
#include <cmath>

namespace LuDash {
namespace {
QJsonObject defaults() {
    return {{"accent", "#7dcccf"}, {"gap", 12}, {"panelHeight", 28},
            {"overview", true}, {"showHostDetails", false}};
}
bool valid(const QString& key, const QJsonValue& value) {
    if (key == "accent") return value.isString() && QRegularExpression("^#[0-9a-fA-F]{6}$").match(value.toString()).hasMatch();
    if (key == "overview" || key == "showHostDetails") return value.isBool();
    if (key == "gap" || key == "panelHeight") {
        const double number = value.toDouble(-1);
        return value.isDouble() && std::isfinite(number) && std::floor(number) == number &&
               number >= (key == "gap" ? 4 : 24) && number <= (key == "gap" ? 32 : 40);
    }
    return false;
}
}
QJsonObject desktopPreferences() {
    auto result = defaults();
    QSettings settings;
    for (auto it = result.begin(); it != result.end(); ++it) {
        auto value = QJsonValue::fromVariant(settings.value("desktop/" + it.key(), it.value().toVariant()));
        // INI settings can return numeric and boolean values as strings after a restart.
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
