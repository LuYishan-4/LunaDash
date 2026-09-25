#pragma once
#include "config/command/CommandRunner.hpp"
#include <QJsonArray>
#include <QJsonObject>
namespace LunaDash {
QJsonObject parseAudioVolume(const QByteArray &text);
QJsonArray parseAudioDevices(const QByteArray &text);
QStringList audioCommand(const QJsonObject &change);
class AudioSettings final : public QObject {
public:
  explicit AudioSettings(QObject *parent = nullptr);
  QJsonObject snapshot() const;
  bool apply(const QJsonObject &change, QString *error);

private:
  CommandRunner outputProbe_, inputProbe_, action_;
  CommandRunner devicesProbe_;
  QString executable_, error_;
  int refreshCount_ = 0;
  QJsonObject output_, input_;
  QJsonArray outputDevices_;
  void refresh();
};
} // namespace LunaDash
