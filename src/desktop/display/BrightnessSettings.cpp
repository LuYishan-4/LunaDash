#include "desktop/display/BrightnessSettings.hpp"
#include <QStandardPaths>
#include <QTimer>

namespace LunaDash {
QJsonObject parseBacklight(const QByteArray &text) {
  for (const auto &line : text.split('\n')) {
    const auto fields = line.trimmed().split(',');
    if (fields.size() != 5 || fields[0].isEmpty() || fields[1] != "backlight" ||
        !fields[3].endsWith('%'))
      continue;
    bool valid = false;
    const int percent = fields[3].chopped(1).toInt(&valid);
    if (valid && percent >= 0 && percent <= 100)
      return {{"available", true},
              {"device", QString::fromUtf8(fields[0])},
              {"percent", percent}};
  }
  return {{"available", false}};
}
BrightnessSettings::BrightnessSettings(QObject *parent) : QObject(parent) {
  executable_ = QStandardPaths::findExecutable("brightnessctl");
  auto *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &BrightnessSettings::refresh);
  timer->start(5000);
  refresh();
}
QJsonObject BrightnessSettings::snapshot() const {
  auto result = data_;
  result["installed"] = !executable_.isEmpty();
  result["busy"] = action_.busy();
  result["error"] = error_;
  return result;
}
void BrightnessSettings::refresh() {
  if (executable_.isEmpty() || action_.busy() || pending_ >= 0)
    return;
  query_.run(executable_, {"-m", "-c", "backlight"},
             [this](bool ok, const QByteArray &text) {
               data_ = ok ? parseBacklight(text)
                          : QJsonObject{{"available", false}};
             });
}
bool BrightnessSettings::setPercent(int percent, QString *error) {
  if (percent < 1 || percent > 100 || !data_.value("available").toBool() ||
      executable_.isEmpty()) {
    *error =
        "Brightness requires an available backlight and a value from 1 to 100.";
    return false;
  }
  pending_ = percent;
  error_.clear();
  applyPending();
  return true;
}
void BrightnessSettings::applyPending() {
  if (pending_ < 0 || action_.busy())
    return;
  const int target = pending_;
  pending_ = -1;
  action_.run(executable_,
              {"-m", "-c", "backlight", "-d", data_.value("device").toString(),
               "set", QString::number(target) + "%"},
              [this](bool ok, const QByteArray &text) {
                if (ok) {
                  const auto updated = parseBacklight(text);
                  if (updated.value("available").toBool())
                    data_ = updated;
                } else {
                  error_ = "Brightness change was refused. Check backlight "
                           "permissions for this session.";
                  pending_ = -1;
                }
                if (pending_ >= 0)
                  applyPending();
                else
                  refresh();
              });
}
} // namespace LunaDash
