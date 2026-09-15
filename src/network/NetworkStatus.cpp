#include <LuDash/network/NetworkStatus.h>
#include <LuDash/process_runner/CommandRunner.h>
#include <QDBusConnection>
#include <QJsonArray>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QNetworkInterface>
#include <QTimer>
#include <QVariantMap>
#include <QStandardPaths>
#include <QRegularExpression>

namespace LuDash {
QJsonObject describeNetwork(unsigned int state, unsigned int connectivity) {
    const bool connected = state >= 50;
    const QString label = connected ? (connectivity == 4 ? "Connected to the Internet" :
        connectivity == 2 ? "Sign-in required" : "Connected; Internet access is unverified") :
        state == 40 ? "Connecting" : "Not connected";
    return {{"managed", true}, {"connected", connected}, {"internet", connected && connectivity == 4}, {"label", label}};
}
NetworkStatus::NetworkStatus(QObject* parent) : QObject(parent), status_({{"label", "Checking network"}, {"managed", false}}) {
    nmcli_ = QStandardPaths::findExecutable("nmcli");
    bluetoothctl_ = QStandardPaths::findExecutable("bluetoothctl");
    command_ = new CommandRunner(this);
    bluetoothCommand_ = new CommandRunner(this);
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &NetworkStatus::refresh);
    timer->start(5000);
    refresh();
}
QJsonObject NetworkStatus::snapshot() const { return status_; }
void NetworkStatus::refresh() {
    if (!bluetoothctl_.isEmpty() && !bluetoothCommand_->busy()) {
        bluetoothCommand_->run(bluetoothctl_, {"devices"},
                               [this](bool ok, const QByteArray& output) {
            QJsonArray devices;
            if (ok) {
                for (const auto& line : QString::fromUtf8(output).split('\n')) {
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
    if (pending_) return;
    pending_ = true;
    auto message = QDBusMessage::createMethodCall("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager",
                                                 "org.freedesktop.DBus.Properties", "GetAll");
    message << "org.freedesktop.NetworkManager";
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 1500), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* call) {
        const QDBusPendingReply<QVariantMap> reply = *call;
        pending_ = false;
        if (!reply.isError()) {
            const auto properties = reply.value();
            status_ = describeNetwork(properties.value("State").toUInt(), properties.value("Connectivity").toUInt());
        } else {
            bool link = false;
            for (const auto& interface : QNetworkInterface::allInterfaces()) {
                const auto flags = interface.flags();
                link = link || (flags.testFlag(QNetworkInterface::IsUp) && flags.testFlag(QNetworkInterface::IsRunning) &&
                                !flags.testFlag(QNetworkInterface::IsLoopBack) && !interface.addressEntries().isEmpty());
            }
            status_ = {{"managed", false}, {"connected", link}, {"internet", false},
                       {"label", link ? "Network link detected; Internet access is unverified" : "NetworkManager is unavailable"}};
        }
        call->deleteLater();
    });
}

void NetworkStatus::refreshNmcli() {
    if (pending_ || command_->busy())
        return;
    pending_ = true;
    command_->run(nmcli_, {"-t", "--escape", "no", "-f", "DEVICE,TYPE,STATE,CONNECTION", "device"},
                  [this](bool ok, const QByteArray& output) {
        pending_ = false;
        QJsonArray devices;
        bool connected = false;
        for (const auto& line : QString::fromUtf8(output).split('\n')) {
            const auto fields = line.split(':');
            if (fields.size() < 4 || fields[0].isEmpty())
                continue;
            connected = connected || fields[2] == "connected";
            devices.append(QJsonObject{{"device", fields[0]}, {"type", fields[1]},
                                       {"state", fields[2]}, {"connection", fields[3]}});
        }
        status_ = {{"managed", ok}, {"connected", connected}, {"internet", false},
                   {"label", ok ? (connected ? "Connected" : "Not connected")
                                : "NetworkManager is unavailable"},
                   {"devices", devices}};
        if (ok)
            command_->run(nmcli_, {"-t", "--escape", "no", "-f",
                                   "NAME,TYPE,DEVICE,STATE", "connection", "show"},
                          [this](bool connectionOk, const QByteArray& connectionOutput) {
                              QJsonArray connections;
                              if (connectionOk) {
                                  for (const auto& line : QString::fromUtf8(connectionOutput).split('\n')) {
                                      const auto fields = line.split(':');
                                      if (fields.size() >= 4 && !fields[0].isEmpty())
                                          connections.append(QJsonObject{{"name", fields[0]},
                                                                         {"type", fields[1]},
                                                                         {"device", fields[2]},
                                                                         {"state", fields[3]}});
                                  }
                              }
                              status_["connections"] = connections;
                              command_->run(nmcli_, {"-t", "--escape", "no", "-f",
                                                     "IN-USE,SSID,SIGNAL,SECURITY,DEVICE", "device", "wifi", "list"},
                                            [this](bool wifiOk, const QByteArray& wifiOutput) {
                              QJsonArray wifi;
                              if (wifiOk) {
                                  for (const auto& line : QString::fromUtf8(wifiOutput).split('\n')) {
                                      const auto fields = line.split(':');
                                      if (fields.size() >= 5 && !fields[1].isEmpty())
                                          wifi.append(QJsonObject{{"active", fields[0] == "*"},
                                                                  {"ssid", fields[1]},
                                                                  {"signal", fields[2].toInt()},
                                                                  {"security", fields[3]},
                                                                  {"device", fields[4]}});
                                  }
                              }
                              status_["wifi"] = wifi;
                          });
                          });
    });
}

bool NetworkStatus::execute(const QJsonObject& request, QString* error) {
    if ((nmcli_.isEmpty() && bluetoothctl_.isEmpty()) ||
        command_->busy() || bluetoothCommand_->busy()) {
        if (error) *error = "NetworkManager/nmcli is unavailable or busy.";
        return false;
    }
    const auto action = request.value("action").toString();
    if (action == "bluetooth-scan" && !bluetoothctl_.isEmpty()) {
        return bluetoothCommand_->run(bluetoothctl_, {"scan", "on"},
                                      [this](bool, const QByteArray&) { refresh(); });
    }
    QStringList args;
    if (action == "wifi-scan") {
        args = {"device", "wifi", "rescan"};
    } else if (action == "wifi-connect" && request.value("ssid").isString() &&
               request.value("password").isString()) {
        args = {"device", "wifi", "connect", request.value("ssid").toString(),
                "password", request.value("password").toString()};
    } else if (action == "connection-up" && request.value("name").isString()) {
        args = {"connection", "up", request.value("name").toString()};
    } else if (action == "connection-down" && request.value("name").isString()) {
        args = {"connection", "down", request.value("name").toString()};
    } else {
        if (error) *error = "Invalid network request.";
        return false;
    }
    return command_->run(nmcli_, args, [this](bool, const QByteArray&) { refresh(); });
}
}
