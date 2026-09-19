#pragma once
#include "config/command/CommandRunner.hpp"
#include <QJsonObject>

namespace LunaDash {
QJsonObject parseBacklight(const QByteArray &text);
class BrightnessSettings final : public QObject {
public:
  explicit BrightnessSettings(QObject *parent = nullptr);
  QJsonObject snapshot() const;
  bool setPercent(int percent, QString *error);
  void refresh();

private:
  CommandRunner query_;
  CommandRunner action_;
  QString executable_;
  QString error_;
  QJsonObject data_;
  int pending_ = -1;
  void applyPending();
};
} // namespace LunaDash
