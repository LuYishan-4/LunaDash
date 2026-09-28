#pragma once
#include "config/command/CommandRunner.hpp"
#include <QJsonObject>
#include <QObject>
#include <QString>
class QTimer;
namespace LunaDash {
QJsonObject describeNetwork(unsigned int state, unsigned int connectivity);
class NetworkStatus final : public QObject {
  Q_OBJECT
public:
  explicit NetworkStatus(QObject *parent = nullptr);
  QJsonObject snapshot() const;
  bool execute(const QJsonObject &request, QString *error);

private Q_SLOTS:
  void scheduleRefresh();

private:
  void refresh();
  void refreshNmcli();
  QJsonObject status_;
  bool pending_ = false;
  CommandRunner *command_ = nullptr;
  CommandRunner *bluetoothCommand_ = nullptr;
  QTimer *eventRefresh_ = nullptr;
  QString nmcli_;
  QString bluetoothctl_;
};
} // namespace LunaDash
