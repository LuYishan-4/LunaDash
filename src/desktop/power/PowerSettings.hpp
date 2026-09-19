#pragma once
#include "config/command/CommandRunner.hpp"
#include <QJsonObject>
namespace LunaDash {
QJsonObject parsePowerProfiles(const QByteArray &text);
class PowerSettings final : public QObject {
public:
  explicit PowerSettings(QObject *parent = nullptr);
  QJsonObject snapshot() const;
  bool apply(const QString &profile, QString *error);

private:
  CommandRunner probe_, action_;
  QString executable_, error_;
  QJsonObject data_;
  void refresh();
};
} // namespace LunaDash
