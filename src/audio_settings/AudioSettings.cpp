#include <LuDash/audio_settings/AudioSettings.h>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <cmath>
#include <algorithm>
namespace LuDash {
QJsonArray parseAudioDevices(const QByteArray& text) {
    QJsonArray devices;
    QString section;
    const QRegularExpression sectionPattern("^\\s*[^A-Za-z]*(Sinks|Sources):\\s*$");
    const QRegularExpression devicePattern(
        "^\\s*.*?(\\*)?\\s*(\\d+)\\.\\s+(.+?)\\s+\\[vol:");
    for (const auto& line : QString::fromUtf8(text).split('\n')) {
        const auto sectionMatch = sectionPattern.match(line);
        if (sectionMatch.hasMatch()) {
            section = sectionMatch.captured(1);
            continue;
        }
        if (section != "Sinks")
            continue;
        const auto match = devicePattern.match(line);
        if (match.hasMatch())
            devices.append(QJsonObject{
                {"id", match.captured(2).toInt()},
                {"name", match.captured(3).trimmed()},
                {"default", !match.captured(1).isEmpty()}});
    }
    return devices;
}

QJsonObject parseAudioVolume(const QByteArray& text) {
    const auto match = QRegularExpression("^Volume: ([0-9]+(?:\\.[0-9]+)?)( \\[MUTED\\])?\\s*$").match(QString::fromUtf8(text));
    if (!match.hasMatch()) return {{"available", false}};
    const double value = match.captured(1).toDouble();
    if (!std::isfinite(value) || value < 0 || value > 10) return {{"available", false}};
    return {{"available", true}, {"volume", std::clamp(static_cast<int>(std::round(value * 100)), 0, 100)}, {"muted", !match.captured(2).isEmpty()}};
}
QStringList audioCommand(const QJsonObject& change) {
    if (change.size() != 2) return {};
    const auto device = change.value("device").toString();
    if (device != "output" && device != "input") return {};
    if (device == "output" && change.value("id").isDouble())
        return {"set-default", QString::number(change.value("id").toInt())};
    const QString target = device == "output" ? "@DEFAULT_AUDIO_SINK@" : "@DEFAULT_AUDIO_SOURCE@";
    if (change.contains("mute") && change.value("mute").isBool()) return {"set-mute", target, change.value("mute").toBool() ? "1" : "0"};
    const auto value = change.value("volume"); const double number = value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(number) || number < 0 || number > 100 || std::floor(number) != number) return {};
    return {"set-volume", target, QString::number(static_cast<int>(number)) + "%", "--limit", "1.0"};
}
AudioSettings::AudioSettings(QObject* parent) : QObject(parent) {
    executable_ = QStandardPaths::findExecutable("wpctl");
    auto* timer = new QTimer(this); timer->setInterval(5000); connect(timer, &QTimer::timeout, this, &AudioSettings::refresh); timer->start(); refresh();
}
void AudioSettings::refresh() {
    if (executable_.isEmpty()) return;
    outputProbe_.run(executable_, {"get-volume", "@DEFAULT_AUDIO_SINK@"}, [this](bool ok, const QByteArray& text) { output_ = ok ? parseAudioVolume(text) : QJsonObject{{"available", false}}; });
    inputProbe_.run(executable_, {"get-volume", "@DEFAULT_AUDIO_SOURCE@"}, [this](bool ok, const QByteArray& text) { input_ = ok ? parseAudioVolume(text) : QJsonObject{{"available", false}}; });
    devicesProbe_.run(executable_, {"status"}, [this](bool ok, const QByteArray& text) { outputDevices_ = ok ? parseAudioDevices(text) : QJsonArray{}; });
}
QJsonObject AudioSettings::snapshot() const { return {{"installed", !executable_.isEmpty()}, {"output", output_}, {"input", input_}, {"outputDevices", outputDevices_}, {"busy", action_.busy()}, {"error", error_}}; }
bool AudioSettings::apply(const QJsonObject& change, QString* error) {
    const auto arguments = audioCommand(change);
    if (arguments.isEmpty() || executable_.isEmpty() || action_.busy()) { if (error) *error = "Invalid audio setting, unavailable WirePlumber, or another audio change is pending."; return false; }
    error_.clear();
    return action_.run(executable_, arguments, [this](bool ok, const QByteArray&) { error_ = ok ? "" : "Audio change failed. Check the selected device and PipeWire service."; refresh(); });
}
}
