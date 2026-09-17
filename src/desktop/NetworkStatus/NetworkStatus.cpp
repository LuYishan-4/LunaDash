#include "desktop/NetworkStatus/NetworkStatus.hpp"
#include "config/CommandRunner/CommandRunner.hpp"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantMap>
#include <algorithm>

namespace LuDash {
namespace {
bool boundedText(const QJsonObject &request, const QString &key, QString *value,
                 int maximum = 512, bool allowEmpty = false) {
  if (!request.value(key).isString())
    return false;
  const QString text = request.value(key).toString();
  if ((!allowEmpty && text.isEmpty()) || text.size() > maximum ||
      text.contains('\n') || text.contains('\r') || text.contains(QChar::Null))
    return false;
  if (value)
    *value = text;
  return true;
}

bool isWifiType(const QString &type) {
  const QString normalized = type.toLower();
  return normalized == "wifi" || normalized == "802-11-wireless" ||
         normalized == "wireless";
}

bool isEthernetType(const QString &type) {
  const QString normalized = type.toLower();
  return normalized == "ethernet" || normalized == "802-3-ethernet" ||
         normalized == "wired";
}

bool wifiSecurityRequiresPassword(const QString &security) {
  const QString normalized = security.trimmed().toLower();
  return !normalized.isEmpty() && normalized != "--" && normalized != "open" &&
         normalized != "none";
}
} // namespace

QJsonObject describeNetwork(unsigned int state, unsigned int connectivity) {
  const bool connected = state >= 50;
  const QString label =
      connected ? (connectivity == 4
                       ? "Connected to the Internet"
                       : connectivity == 2
                             ? "Sign-in required"
                             : "Connected; Internet access is unverified")
                : state == 40 ? "Connecting" : "Not connected";
  return {{"managed", true},
          {"connected", connected},
          {"internet", connected && connectivity == 4},
          {"label", label},
          {"hasWifi", false},
          {"wifiEnabled", false},
          {"wifiConnected", false},
          {"ethernetConnected", false},
          {"primaryType", connected ? "network" : "disconnected"}};
}

NetworkStatus::NetworkStatus(QObject *parent)
    : QObject(parent),
      status_({{"label", "Checking network"}, {"managed", false}}) {
  nmcli_ = QStandardPaths::findExecutable("nmcli");
  bluetoothctl_ = QStandardPaths::findExecutable("bluetoothctl");
  command_ = new CommandRunner(this);
  bluetoothCommand_ = new CommandRunner(this);
  auto *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &NetworkStatus::refresh);
  timer->start(5000);
  refresh();
}

QJsonObject NetworkStatus::snapshot() const { return status_; }

void NetworkStatus::refresh() {
  if (!bluetoothctl_.isEmpty() && !bluetoothCommand_->busy()) {
    bluetoothCommand_->run(bluetoothctl_, {"devices"},
                           [this](bool ok, const QByteArray &output) {
      QJsonArray devices;
      if (ok) {
        for (const auto &line : QString::fromUtf8(output).split('\n')) {
          const auto fields = line.split(' ', Qt::SkipEmptyParts);
          if (fields.size() >= 3 && fields[0] == "Device")
            devices.append(QJsonObject{{"address", fields[1]},
                                       {"name", fields.mid(2).join(" ")}});
        }
      }
      status_["bluetooth"] = devices;
    });
  }
  if (!nmcli_.isEmpty()) {
    refreshNmcli();
    return;
  }
  if (pending_)
    return;
  pending_ = true;
  auto message = QDBusMessage::createMethodCall(
      "org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager",
      "org.freedesktop.DBus.Properties", "GetAll");
  message << "org.freedesktop.NetworkManager";
  auto *watcher = new QDBusPendingCallWatcher(
      QDBusConnection::systemBus().asyncCall(message, 1500), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this](QDBusPendingCallWatcher *call) {
    const QDBusPendingReply<QVariantMap> reply = *call;
    pending_ = false;
    if (!reply.isError()) {
      const auto properties = reply.value();
      status_ = describeNetwork(properties.value("State").toUInt(),
                                properties.value("Connectivity").toUInt());
    } else {
      bool link = false;
      for (const auto &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        link = link ||
               (flags.testFlag(QNetworkInterface::IsUp) &&
                flags.testFlag(QNetworkInterface::IsRunning) &&
                !flags.testFlag(QNetworkInterface::IsLoopBack) &&
                !interface.addressEntries().isEmpty());
      }
      status_ = {{"managed", false},
                 {"connected", link},
                 {"internet", false},
                 {"label", link ? "Network link detected; Internet access is unverified"
                                : "NetworkManager is unavailable"},
                 {"hasWifi", false},
                 {"wifiEnabled", false},
                 {"wifiConnected", false},
                 {"ethernetConnected", link},
                 {"primaryType", link ? "ethernet" : "disconnected"}};
    }
    call->deleteLater();
  });
}

void NetworkStatus::refreshNmcli() {
  if (pending_ || command_->busy())
    return;
  pending_ = true;
  command_->run(nmcli_,
                {"-t", "--escape", "no", "-f", "DEVICE,TYPE,STATE,CONNECTION",
                 "device"},
                [this](bool ok, const QByteArray &output) {
    pending_ = false;
    QJsonArray devices;
    bool connected = false;
    bool hasWifi = false;
    bool wifiEnabled = false;
    bool wifiConnected = false;
    bool ethernetConnected = false;
    QString primaryConnection;
    QString primaryType = "disconnected";

    for (const auto &line : QString::fromUtf8(output).split('\n')) {
      const auto fields = line.split(':');
      if (fields.size() < 4 || fields[0].isEmpty())
        continue;
      const QString deviceName = fields[0];
      const QString type = fields[1];
      const QString state = fields[2];
      const QString connection = fields[3];
      const bool deviceConnected = state == "connected";
      connected = connected || deviceConnected;

      if (isWifiType(type)) {
        hasWifi = true;
        wifiEnabled = wifiEnabled || state != "unavailable";
        if (deviceConnected) {
          wifiConnected = true;
          if (primaryConnection.isEmpty()) {
            primaryConnection = connection;
            primaryType = "wifi";
          }
        }
      } else if (isEthernetType(type) && deviceConnected) {
        ethernetConnected = true;
        primaryConnection = connection;
        primaryType = "ethernet";
      }

      devices.append(QJsonObject{{"device", deviceName},
                                 {"type", type},
                                 {"state", state},
                                 {"connection", connection}});
    }

    status_ = {{"managed", ok},
               {"connected", connected},
               {"internet", false},
               {"label", ok ? (connected ? "Connected" : "Not connected")
                            : "NetworkManager is unavailable"},
               {"devices", devices},
               {"hasWifi", hasWifi},
               {"wifiEnabled", hasWifi && wifiEnabled},
               {"wifiConnected", wifiConnected},
               {"ethernetConnected", ethernetConnected},
               {"primaryType", primaryType},
               {"primaryConnection", primaryConnection}};
    if (!ok)
      return;

    command_->run(
        nmcli_,
        {"-t", "--escape", "no", "-f",
         "NAME,UUID,TYPE,DEVICE,STATE,AUTOCONNECT", "connection", "show"},
        [this](bool connectionOk, const QByteArray &connectionOutput) {
      QJsonArray connections;
      if (connectionOk) {
        for (const auto &line : QString::fromUtf8(connectionOutput).split('\n')) {
          const auto fields = line.split(':');
          if (fields.size() >= 6 && !fields[0].isEmpty())
            connections.append(QJsonObject{{"name", fields[0]},
                                           {"uuid", fields[1]},
                                           {"type", fields[2]},
                                           {"device", fields[3]},
                                           {"state", fields[4]},
                                           {"autoconnect", fields[5] == "yes"}});
        }
      }
      status_["connections"] = connections;

      if (!status_.value("hasWifi").toBool()) {
        status_["wifi"] = QJsonArray{};
        return;
      }

      command_->run(
          nmcli_,
          {"-t", "--escape", "no", "-f",
           "IN-USE,SSID,SIGNAL,SECURITY,DEVICE", "device", "wifi", "list"},
          [this](bool wifiOk, const QByteArray &wifiOutput) {
        QMap<QString, QJsonObject> strongestBySsid;
        if (wifiOk) {
          for (const auto &line : QString::fromUtf8(wifiOutput).split('\n')) {
            const auto fields = line.split(':');
            if (fields.size() < 5 || fields[1].isEmpty())
              continue;
            const QString ssid = fields[1];
            const int signal = fields[2].toInt();
            QJsonObject candidate{{"active", fields[0] == "*"},
                                  {"ssid", ssid},
                                  {"signal", signal},
                                  {"security", fields[3]},
                                  {"secured", wifiSecurityRequiresPassword(fields[3])},
                                  {"device", fields[4]}};
            if (!strongestBySsid.contains(ssid) ||
                strongestBySsid.value(ssid).value("signal").toInt() < signal ||
                candidate.value("active").toBool())
              strongestBySsid[ssid] = candidate;
          }
        }
        QList<QJsonObject> sorted = strongestBySsid.values();
        std::sort(sorted.begin(), sorted.end(), [](const QJsonObject &a,
                                                    const QJsonObject &b) {
          if (a.value("active").toBool() != b.value("active").toBool())
            return a.value("active").toBool();
          return a.value("signal").toInt() > b.value("signal").toInt();
        });
        QJsonArray wifi;
        for (const auto &entry : sorted)
          wifi.append(entry);
        status_["wifi"] = wifi;
      });
    });
  });
}

bool NetworkStatus::execute(const QJsonObject &request, QString *error) {
  const auto action = request.value("action").toString();
  if (action == "bluetooth-scan") {
    if (bluetoothctl_.isEmpty() || bluetoothCommand_->busy()) {
      if (error)
        *error = "Bluetooth control is unavailable or busy.";
      return false;
    }
    return bluetoothCommand_->run(
        bluetoothctl_, {"scan", "on"},
        [this](bool, const QByteArray &) { refresh(); });
  }

  if (nmcli_.isEmpty() || command_->busy()) {
    if (error)
      *error = "NetworkManager/nmcli is unavailable or busy.";
    return false;
  }

  QStringList args;
  QString name;
  QString device;
  if (action == "network-reset") {
    return command_->run(nmcli_, {"networking", "off"},
                         [this](bool, const QByteArray &) {
      command_->run(nmcli_, {"networking", "on"},
                    [this](bool, const QByteArray &) { refresh(); });
    });
  }
  if (action == "connection-reconnect" &&
      boundedText(request, "name", &name, 256)) {
    return command_->run(nmcli_, {"connection", "down", name},
                         [this, name](bool, const QByteArray &) {
      command_->run(nmcli_, {"connection", "up", name},
                    [this](bool, const QByteArray &) { refresh(); });
    });
  }
  if (action == "wifi-scan") {
    if (!status_.value("hasWifi").toBool()) {
      if (error)
        *error = "No wireless network adapter is available.";
      return false;
    }
    args = {"device", "wifi", "rescan"};
  } else if (action == "wifi-connect") {
    QString ssid;
    QString password;
    if (!boundedText(request, "ssid", &ssid, 128) ||
        !boundedText(request, "password", &password, 512, true)) {
      if (error)
        *error = "Invalid Wi-Fi request.";
      return false;
    }
    args = {"device", "wifi", "connect", ssid};
    if (!password.isEmpty())
      args << "password" << password;
  } else if ((action == "connection-up" || action == "connection-down" ||
              action == "connection-delete") &&
             boundedText(request, "name", &name, 256)) {
    args = {"connection",
            action == "connection-up"
                ? "up"
                : action == "connection-down" ? "down" : "delete",
            name};
  } else if ((action == "device-connect" || action == "device-disconnect") &&
             boundedText(request, "device", &device, 128)) {
    args = {"device", action == "device-connect" ? "connect" : "disconnect",
            device};
  } else if ((action == "networking-enable" || action == "wifi-enable") &&
             request.value("enabled").isBool()) {
    const QString value = request.value("enabled").toBool() ? "on" : "off";
    args = action == "networking-enable" ? QStringList{"networking", value}
                                          : QStringList{"radio", "wifi", value};
  } else if (action == "connection-autoconnect" &&
             boundedText(request, "name", &name, 256) &&
             request.value("enabled").isBool()) {
    args = {"connection", "modify", name, "connection.autoconnect",
            request.value("enabled").toBool() ? "yes" : "no"};
  } else if (action == "connection-ipv4-auto" &&
             boundedText(request, "name", &name, 256)) {
    args = {"connection", "modify", name,
            "ipv4.method", "auto",
            "ipv4.addresses", "",
            "ipv4.gateway", "",
            "ipv4.dns", "",
            "ipv4.ignore-auto-dns", "no"};
  } else if (action == "connection-ipv4-manual" &&
             boundedText(request, "name", &name, 256)) {
    QString address;
    QString gateway;
    QString dns;
    if (!boundedText(request, "address", &address, 128) ||
        !boundedText(request, "gateway", &gateway, 128, true) ||
        !boundedText(request, "dns", &dns, 256, true)) {
      if (error)
        *error = "Invalid IPv4 settings.";
      return false;
    }
    args = {"connection", "modify", name, "ipv4.method", "manual",
            "ipv4.addresses", address, "ipv4.gateway", gateway, "ipv4.dns", dns,
            "ipv4.ignore-auto-dns", dns.isEmpty() ? "no" : "yes"};
  } else if ((action == "connection-ipv6-auto" ||
              action == "connection-ipv6-disabled") &&
             boundedText(request, "name", &name, 256)) {
    args = {"connection", "modify", name, "ipv6.method",
            action == "connection-ipv6-auto" ? "auto" : "disabled"};
  } else if (action == "connection-route-metric" &&
             boundedText(request, "name", &name, 256) &&
             request.value("metric").isDouble()) {
    const int metric = request.value("metric").toInt(-1);
    if (metric < -1 || metric > 65535) {
      if (error)
        *error = "Invalid route metric.";
      return false;
    }
    args = {"connection", "modify", name, "ipv4.route-metric",
            QString::number(metric)};
  } else {
    if (error)
      *error = "Invalid network request.";
    return false;
  }

  return command_->run(nmcli_, args,
                       [this](bool, const QByteArray &) { refresh(); });
}
} // namespace LuDash
