#include "desktop/display/DdcBrightnessSettings.hpp"
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QtMath>

namespace LunaDash {
QJsonArray parseDdcDisplays(const QByteArray &text) {
  QJsonArray result;
  QSet<int> buses;
  QJsonObject device;
  bool valid = false;
  static const QRegularExpression display("^Display [1-9][0-9]*$");
  static const QRegularExpression bus("^I2C bus:\\s+/dev/i2c-([0-9]+)$");
  auto append = [&] {
    if (!valid || !device.contains("bus") || !device.contains("label"))
      return;
    const int number = device.value("bus").toInt();
    if (buses.contains(number) || result.size() >= 16)
      return;
    buses.insert(number);
    device["id"] =
        QString("i2c-%1:%2").arg(number).arg(device.value("label").toString());
    device["available"] = false;
    device["percent"] = 0;
    device["error"] = QString();
    result.append(device);
  };
  for (const auto &raw : text.split('\n')) {
    const QString line = QString::fromUtf8(raw).trimmed();
    if (display.match(line).hasMatch() || line.startsWith("Invalid display")) {
      append();
      device = {};
      valid = display.match(line).hasMatch();
    } else if (valid) {
      const auto match = bus.match(line);
      if (match.hasMatch()) {
        bool ok = false;
        const int number = match.captured(1).toInt(&ok);
        if (ok)
          device["bus"] = number;
      } else if (line.startsWith("Monitor:")) {
        const auto label = line.mid(8).trimmed();
        if (!label.isEmpty())
          device["label"] = label;
      }
    }
  }
  append();
  return result;
}

QJsonObject parseDdcBrightness(const QByteArray &text) {
  static const QRegularExpression value("^VCP 10 C ([0-9]+) ([0-9]+)$");
  for (const auto &line : text.split('\n')) {
    const auto match = value.match(QString::fromUtf8(line).simplified());
    if (!match.hasMatch())
      continue;
    bool currentOk = false, maximumOk = false;
    const int current = match.captured(1).toInt(&currentOk);
    const int maximum = match.captured(2).toInt(&maximumOk);
    if (currentOk && maximumOk && maximum > 0 && maximum <= 65535 &&
        current <= maximum)
      return {{"available", true},
              {"maximum", maximum},
              {"percent", qRound(current * 100.0 / maximum)}};
  }
  return {{"available", false}};
}

DdcBrightnessSettings::DdcBrightnessSettings(QObject *parent)
    : QObject(parent) {
  auto *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &DdcBrightnessSettings::refresh);
  timer->start(60000);
  QTimer::singleShot(0, this, &DdcBrightnessSettings::refresh);
}

QJsonObject DdcBrightnessSettings::snapshot() const {
  QJsonArray devices;
  for (auto device : devices_) {
    const auto id = device.value("id").toString();
    device["busy"] = id == activeId_ || pending_.contains(id);
    devices.append(device);
  }
  return {{"installed", !executable_.isEmpty()},
          {"scanning", scanning_},
          {"busy", runner_.busy()},
          {"error", error_},
          {"devices", devices}};
}

void DdcBrightnessSettings::refresh() {
  if (scanning_)
    return;
  if (runner_.busy()) {
    refreshRequested_ = true;
    return;
  }
  executable_ = QStandardPaths::findExecutable("ddcutil");
  if (executable_.isEmpty()) {
    devices_.clear();
    error_ = "Install ddcutil to control external monitor brightness.";
    return;
  }
  refreshRequested_ = true;
  advance();
}

bool DdcBrightnessSettings::setPercent(const QString &id, int percent,
                                       QString *error) {
  for (const auto &device : devices_) {
    if (device.value("id") != id)
      continue;
    if (percent < 0 || percent > 100 || scanning_ ||
        !device.value("available").toBool() || executable_.isEmpty())
      break;
    pending_[id] = percent;
    advance();
    return true;
  }
  *error = "Choose an available DDC/CI monitor and a brightness from 0 to 100.";
  return false;
}

void DdcBrightnessSettings::advance() {
  if (runner_.busy())
    return;
  activeId_.clear();
  if (!pending_.isEmpty()) {
    const auto id = pending_.firstKey();
    const int percent = pending_.take(id);
    for (int index = 0; index < devices_.size(); ++index) {
      auto &device = devices_[index];
      if (device.value("id") != id)
        continue;
      activeId_ = id;
      device["error"] = QString();
      const int maximum = device.value("maximum").toInt();
      const int value = qRound(percent * maximum / 100.0);
      // One helper at a time avoids concurrent I2C requests. Keep verification
      // enabled: an acknowledged write alone does not prove brightness changed.
      runner_.run(
          executable_,
          {"setvcp", "10", QString::number(value), "--bus",
           QString::number(device.value("bus").toInt()), "--verify"},
          [this, index, id, value, maximum](bool ok, const QByteArray &) {
            auto &updated = devices_[index];
            if (ok)
              updated["percent"] = qRound(value * 100.0 / maximum);
            else {
              updated["error"] =
                  "DDC/CI brightness change failed. Check the monitor "
                  "connection, DDC/CI setting and I2C permissions.";
              pending_.remove(id);
            }
            advance();
          },
          12000);
      return;
    }
    advance();
    return;
  }
  if (queryIndex_ >= 0) {
    if (queryIndex_ >= devices_.size()) {
      queryIndex_ = -1;
      scanning_ = false;
      return;
    }
    const int index = queryIndex_++;
    const auto device = devices_[index];
    activeId_ = device.value("id").toString();
    runner_.run(
        executable_,
        {"getvcp", "10", "--brief", "--bus",
         QString::number(device.value("bus").toInt())},
        [this, index](bool ok, const QByteArray &output) {
          const auto value = parseDdcBrightness(output);
          auto &device = devices_[index];
          const bool available = ok && value.value("available").toBool();
          device["available"] = available;
          if (available) {
            device["maximum"] = value.value("maximum");
            device["percent"] = value.value("percent");
          } else
            device["error"] =
                "This monitor did not report DDC/CI brightness. Enable DDC/CI "
                "and check I2C permissions, then refresh.";
          advance();
        },
        12000);
    return;
  }
  if (!refreshRequested_)
    return;
  refreshRequested_ = false;
  scanning_ = true;
  error_.clear();
  runner_.run(
      executable_, {"detect", "--brief", "--disable-usb"},
      [this](bool ok, const QByteArray &output) {
        devices_.clear();
        // detect can exit unsuccessfully while still reporting other
        // valid displays. Invalid display blocks are never controllable.
        for (const auto &device : parseDdcDisplays(output))
          devices_.append(device.toObject());
        if (!ok)
          error_ = "DDC/CI detection failed for one or more displays. Check "
                   "DDC/CI, the i2c-dev module and I2C permissions.";
        queryIndex_ = 0;
        advance();
      },
      20000);
}
} // namespace LunaDash
