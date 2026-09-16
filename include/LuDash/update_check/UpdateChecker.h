#pragma once

#include <QJsonObject>
#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace LuDash {

class UpdateChecker final : public QObject {
  Q_OBJECT

public:
  explicit UpdateChecker(QObject *parent = nullptr);
  QJsonObject snapshot() const;
  void check();
  bool setChannel(const QString &channel);

signals:
  void changed();

private:
  void finishWithError(const QString &message);
  void finishReply();

  QNetworkAccessManager *network_ = nullptr;
  QNetworkReply *reply_ = nullptr;
  QTimer *timeout_ = nullptr;
  QString channel_ = "stable";
  QString status_ = "idle";
  QString latestVersion_;
  QString latestCommit_;
  QString latestMessage_;
  QString releaseUrl_;
  QString error_;
  QString checkedAt_;
};

} // namespace LuDash
