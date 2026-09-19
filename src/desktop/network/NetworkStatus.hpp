#pragma once
#include "config/command/CommandRunner.hpp"
#include <QJsonObject>
#include <QObject>
#include <QString>
namespace LunaDash {
QJsonObject describeNetwork(unsigned int state, unsigned int connectivity);
class NetworkStatus final : public QObject {
public:
  explicit NetworkStatus(QObject *parent = nullptr);
  QJsonObject snapshot() const;
  bool execute(const QJsonObject &request, QString *error);

private:
  void refresh();
  void refreshNmcli();
  QJsonObject status_;
  bool pending_ = false;
  CommandRunner *command_ = nullptr;
  CommandRunner *bluetoothCommand_ = nullptr;
  QString nmcli_;
  QString bluetoothctl_;
};
} // namespace LunaDash
