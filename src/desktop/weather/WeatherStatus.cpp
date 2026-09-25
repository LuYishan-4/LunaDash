#include "desktop/weather/WeatherStatus.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QUrlQuery>
#include <cmath>

namespace LunaDash {
namespace {
QString condition(int code) {
  if (code == 0)
    return "Clear";
  if (code == 1 || code == 2)
    return "Partly cloudy";
  if (code == 3)
    return "Overcast";
  if (code == 45 || code == 48)
    return "Fog";
  if (code >= 51 && code <= 57)
    return "Drizzle";
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82))
    return "Rain";
  if ((code >= 71 && code <= 77) || code == 85 || code == 86)
    return "Snow";
  if (code >= 95 && code <= 99)
    return "Thunderstorm";
  return "Weather data unavailable";
}
} // namespace
WeatherStatus::WeatherStatus(QObject *parent) : QObject(parent) {
  connect(&timer_, &QTimer::timeout, this, &WeatherStatus::refresh);
  timer_.setInterval(15 * 60 * 1000);
  timer_.start();
  refresh();
}
QJsonObject WeatherStatus::snapshot() const { return state_; }
void WeatherStatus::refresh() {
  const auto preferences = desktopPreferences();
  const bool enabled = preferences.value("weatherEnabled").toBool();
  const auto latitude =
      QString::number(preferences.value("weatherLatitude").toDouble(), 'f', 3);
  const auto longitude =
      QString::number(preferences.value("weatherLongitude").toDouble(), 'f', 3);
  const auto location = preferences.value("weatherLocation").toString();
  const auto next = latitude + "," + longitude + "," + location;
  if (reply_ && (configuration_ != next || !enabled)) {
    reply_->disconnect(this);
    reply_->abort();
    reply_->deleteLater();
    reply_ = nullptr;
  }
  if (!enabled) {
    state_ = {{"enabled", false}, {"available", false}};
    configuration_.clear();
    return;
  }
  if (reply_)
    return;
  if (configuration_ != next)
    state_ = {};
  configuration_ = next;
  state_["enabled"] = true;
  state_["location"] = location;
  state_["provider"] = "Open-Meteo";
  QUrl url("https://api.open-meteo.com/v1/forecast");
  QUrlQuery query;
  query.addQueryItem("latitude", latitude);
  query.addQueryItem("longitude", longitude);
  query.addQueryItem("current", "temperature_2m,weather_code");
  query.addQueryItem("forecast_days", "1");
  url.setQuery(query);
  QNetworkRequest request(url);
  request.setTransferTimeout(15000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::SameOriginRedirectPolicy);
  request.setRawHeader("User-Agent", "LunaDash/1.0.1a");
  auto *reply = network_.get(request);
  reply_ = reply;
  reply->setReadBufferSize(65537);
  connect(reply, &QIODevice::readyRead, this, [reply] {
    if (reply->bytesAvailable() > 65536)
      reply->abort();
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto bytes = reply->readAll();
    const auto current =
        QJsonDocument::fromJson(bytes).object().value("current").toObject();
    const auto temperature = current.value("temperature_2m");
    const auto weatherCode = current.value("weather_code");
    if (reply->error() == QNetworkReply::NoError && bytes.size() <= 65536 &&
        temperature.isDouble() && std::isfinite(temperature.toDouble()) &&
        temperature.toDouble() >= -150 && temperature.toDouble() <= 150 &&
        weatherCode.isDouble()) {
      state_["available"] = true;
      state_["temperature"] = std::round(temperature.toDouble());
      state_["condition"] = condition(weatherCode.toInt());
      state_["updatedAt"] =
          QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      state_["error"] = "";
    } else {
      state_["error"] = "Weather data unavailable";
      state_["available"] = false;
    }
    reply_ = nullptr;
    reply->deleteLater();
  });
}
} // namespace LunaDash
