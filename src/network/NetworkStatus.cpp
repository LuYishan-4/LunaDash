#include <LuDash/network/NetworkStatus.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QNetworkInterface>
#include <QTimer>
#include <QVariantMap>

namespace LuDash {
QJsonObject describeNetwork(unsigned int state, unsigned int connectivity) {
    const bool connected = state >= 50;
    const QString label = connected ? (connectivity == 4 ? "Connected to the Internet" :
        connectivity == 2 ? "Sign-in required" : "Connected; Internet access is unverified") :
        state == 40 ? "Connecting" : "Not connected";
    return {{"managed", true}, {"connected", connected}, {"internet", connected && connectivity == 4}, {"label", label}};
}
NetworkStatus::NetworkStatus(QObject* parent) : QObject(parent), status_({{"label", "Checking network"}, {"managed", false}}) {
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &NetworkStatus::refresh);
    timer->start(5000);
    refresh();
}
QJsonObject NetworkStatus::snapshot() const { return status_; }
void NetworkStatus::refresh() {
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
}
