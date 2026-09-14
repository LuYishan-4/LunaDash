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

signals:
  void changed();

private:
  void finishWithError(const QString &message);
  void finishReply();

  QNetworkAccessManager *network_ = nullptr;
  QNetworkReply *reply_ = nullptr;
  QTimer *timeout_ = nullptr;
  QString status_ = "idle";
  QString latestVersion_;
  QString releaseUrl_;
  QString error_;
  QString checkedAt_;
};

} // namespace LuDash
