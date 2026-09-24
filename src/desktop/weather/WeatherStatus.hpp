#pragma once
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>

class QNetworkReply;
namespace LunaDash {
class WeatherStatus final : public QObject {
public:
  explicit WeatherStatus(QObject *parent = nullptr);
  QJsonObject snapshot() const;
  void refresh();

private:
  QNetworkAccessManager network_;
  QPointer<QNetworkReply> reply_;
  QTimer timer_;
  QJsonObject state_;
  QString configuration_;
};
} // namespace LunaDash
