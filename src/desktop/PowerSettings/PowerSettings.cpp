#include "desktop/PowerSettings/PowerSettings.hpp"
#include <QJsonArray>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
namespace LuDash {
QJsonObject parsePowerProfiles(const QByteArray& text) {
    QJsonArray profiles; QString current;
    const QRegularExpression expression("^\\s*(\\*)?\\s*(power-saver|balanced|performance):\\s*$");
    for (const auto& line : text.split('\n')) { const auto match = expression.match(QString::fromUtf8(line)); if (!match.hasMatch()) continue;
        profiles.append(match.captured(2)); if (!match.captured(1).isEmpty()) current = match.captured(2);
    }
    return {{"available", !profiles.isEmpty()}, {"profiles", profiles}, {"current", current}};
}
PowerSettings::PowerSettings(QObject* parent) : QObject(parent) {
    executable_ = QStandardPaths::findExecutable("powerprofilesctl");
    auto* timer = new QTimer(this); timer->setInterval(10000); connect(timer, &QTimer::timeout, this, &PowerSettings::refresh); timer->start(); refresh();
}
void PowerSettings::refresh() { if (!executable_.isEmpty()) probe_.run(executable_, {"list"}, [this](bool ok, const QByteArray& text) { data_ = ok ? parsePowerProfiles(text) : QJsonObject{{"available", false}}; }); }
QJsonObject PowerSettings::snapshot() const { auto result = data_; result["installed"] = !executable_.isEmpty(); result["busy"] = action_.busy(); result["error"] = error_; return result; }
bool PowerSettings::apply(const QString& profile, QString* error) {
    if (!data_.value("profiles").toArray().contains(profile) || executable_.isEmpty() || action_.busy()) { if (error) *error = "Power profile is unavailable or another change is pending."; return false; }
    error_.clear(); return action_.run(executable_, {"set", profile}, [this](bool ok, const QByteArray&) { error_ = ok ? "" : "Power profile change was refused by the system service."; refresh(); });
}
}
