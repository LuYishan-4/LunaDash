#pragma once
#include "config/CommandRunner/CommandRunner.hpp"
#include <QJsonObject>
#include <QJsonArray>
namespace LuDash {
QJsonObject parseAudioVolume(const QByteArray& text);
QJsonArray parseAudioDevices(const QByteArray& text);
QStringList audioCommand(const QJsonObject& change);
class AudioSettings final : public QObject {
public:
    explicit AudioSettings(QObject* parent = nullptr);
    QJsonObject snapshot() const;
    bool apply(const QJsonObject& change, QString* error);
private:
    CommandRunner outputProbe_, inputProbe_, action_;
    CommandRunner devicesProbe_;
    QString executable_, error_;
    QJsonObject output_, input_;
    QJsonArray outputDevices_;
    void refresh();
};
}
