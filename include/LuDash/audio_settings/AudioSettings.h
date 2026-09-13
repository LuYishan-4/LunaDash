#pragma once
#include <LuDash/process_runner/CommandRunner.h>
#include <QJsonObject>
namespace LuDash {
QJsonObject parseAudioVolume(const QByteArray& text);
QStringList audioCommand(const QJsonObject& change);
class AudioSettings final : public QObject {
public:
    explicit AudioSettings(QObject* parent = nullptr);
    QJsonObject snapshot() const;
    bool apply(const QJsonObject& change, QString* error);
private:
    CommandRunner outputProbe_, inputProbe_, action_;
    QString executable_, error_;
    QJsonObject output_, input_;
    void refresh();
};
}
