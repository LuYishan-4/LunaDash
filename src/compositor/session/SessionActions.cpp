#include "compositor/session/SessionActions.hpp"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QStringList>

namespace LunaDash {
namespace {
constexpr auto service = "org.freedesktop.login1";
constexpr auto path = "/org/freedesktop/login1";
constexpr auto interface = "org.freedesktop.login1.Manager";

QString dbusMethod(const QString &action) {
  if (action == "suspend")
    return "Suspend";
  if (action == "reboot")
    return "Reboot";
  if (action == "poweroff")
    return "PowerOff";
  return {};
}

QString canMethod(const QString &action) { return "Can" + dbusMethod(action); }
} // namespace

bool isValidSessionAction(const QString &action) {
  return action == "suspend" || action == "reboot" || action == "poweroff";
}

SessionActions::SessionActions(QObject *parent) : QObject(parent) {
  refreshAvailability();
}

void SessionActions::refreshAvailability() {
  availability_ = {{"suspend", false}, {"reboot", false}, {"poweroff", false}};
  availabilityError_.clear();
  QDBusInterface manager(service, path, interface,
                         QDBusConnection::systemBus());
  if (!manager.isValid()) {
    availabilityError_ = manager.lastError().message();
    if (availabilityError_.isEmpty())
      availabilityError_ = "systemd-logind is unavailable.";
    return;
  }

  for (const auto &action : QStringList{"suspend", "reboot", "poweroff"}) {
    const QDBusReply<QString> reply = manager.call(canMethod(action));
    if (!reply.isValid()) {
      if (availabilityError_.isEmpty())
        availabilityError_ = reply.error().message();
      continue;
    }
    const auto permission = reply.value();
    availability_[action] = permission == "yes" || permission == "challenge";
  }
}

QJsonObject SessionActions::snapshot() const {
  QJsonObject result = availability_;
  result["error"] = availabilityError_;
  return result;
}

bool SessionActions::execute(const QString &action, QString *error) {
  if (!isValidSessionAction(action)) {
    if (error)
      *error = "Invalid session action.";
    return false;
  }
  if (!availability_.value(action).toBool()) {
    if (error)
      *error = availabilityError_.isEmpty()
                   ? "This session action is unavailable."
                   : availabilityError_;
    return false;
  }

  QDBusInterface manager(service, path, interface,
                         QDBusConnection::systemBus());
  const QDBusReply<void> reply = manager.call(dbusMethod(action), true);
  if (!reply.isValid()) {
    if (error)
      *error = reply.error().message().isEmpty() ? "The session action failed."
                                                 : reply.error().message();
    refreshAvailability();
    return false;
  }
  return true;
}

} // namespace LunaDash
