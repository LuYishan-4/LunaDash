#pragma once
#include "config/command/CommandRunner.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

namespace LunaDash {
QJsonArray parseDdcDisplays(const QByteArray &text);
QJsonObject parseDdcBrightness(const QByteArray &text);

class DdcBrightnessSettings final : public QObject {
public:
  explicit DdcBrightnessSettings(QObject *parent = nullptr);
  QJsonObject snapshot() const;
  bool setPercent(const QString &id, int percent, QString *error);
  void refresh();

private:
  CommandRunner runner_;
  QString executable_;
  QString error_;
  QString activeId_;
  QList<QJsonObject> devices_;
  QMap<QString, int> pending_;
  bool refreshRequested_ = false;
  bool scanning_ = false;
  int queryIndex_ = -1;
  void advance();
};
} // namespace LunaDash
