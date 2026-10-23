#include "compositor/session/SessionActions.hpp"

#include <gio/gio.h>

#include <array>
#include <memory>
#include <utility>

namespace LunaDash {
namespace {
constexpr char service[] = "org.freedesktop.login1";
constexpr char path[] = "/org/freedesktop/login1";
constexpr char interface[] = "org.freedesktop.login1.Manager";
constexpr std::array<std::string_view, 3> actions = {"suspend", "reboot",
                                                    "poweroff"};

using Connection = std::unique_ptr<GDBusConnection, decltype(&g_object_unref)>;
using Variant = std::unique_ptr<GVariant, decltype(&g_variant_unref)>;
Connection systemBus(std::string *errorMessage) {
  GError *error = nullptr;
  Connection connection(g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error),
                        &g_object_unref);
  if (!connection && errorMessage) {
    if (error && error->message)
      *errorMessage = error->message;
    else
      *errorMessage = "Could not connect to the system D-Bus.";
  }
  if (error)
    g_error_free(error);
  return connection;
}

Variant call(GDBusConnection *connection, const char *method,
             GVariant *parameters, const GVariantType *replyType,
             std::string *errorMessage) {
  GError *error = nullptr;
  Variant reply(g_dbus_connection_call_sync(
                    connection, service, path, interface, method, parameters,
                    replyType, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error),
                &g_variant_unref);
  if (!reply && errorMessage) {
    if (error && error->message)
      *errorMessage = error->message;
    else
      *errorMessage = "The system D-Bus request failed.";
  }
  if (error)
    g_error_free(error);
  return reply;
}

const char *dbusMethod(std::string_view action) {
  if (action == "suspend")
    return "Suspend";
  if (action == "reboot")
    return "Reboot";
  if (action == "poweroff")
    return "PowerOff";
  return nullptr;
}

} // namespace

bool isValidSessionAction(std::string_view action) {
  return action == "suspend" || action == "reboot" || action == "poweroff";
}

SessionActions::SessionActions() {
  refreshAvailability();
}

void SessionActions::refreshAvailability() {
  availability_.clear();
  for (const auto action : actions)
    availability_.emplace(action, false);
  availabilityError_.clear();

  auto connection = systemBus(&availabilityError_);
  if (!connection)
    return;

  for (const auto action : actions) {
    const std::string method = "Can" + std::string(dbusMethod(action));
    std::string error;
    auto reply = call(connection.get(), method.c_str(), nullptr,
                      G_VARIANT_TYPE("(s)"), &error);
    if (!reply) {
      if (availabilityError_.empty())
        availabilityError_ = std::move(error);
      continue;
    }
    const char *permission = nullptr;
    g_variant_get(reply.get(), "(&s)", &permission);
    availability_[std::string(action)] =
        permission && (std::string_view(permission) == "yes" ||
                       std::string_view(permission) == "challenge");
  }
}

SessionActionSnapshot SessionActions::snapshot() const {
  return {availability_, availabilityError_};
}

bool SessionActions::execute(std::string_view action, std::string *error) {
  if (!isValidSessionAction(action)) {
    if (error)
      *error = "Invalid session action.";
    return false;
  }
  const auto available = availability_.find(std::string(action));
  if (available == availability_.end() || !available->second) {
    if (error)
      *error = availabilityError_.empty()
                   ? "This session action is unavailable."
                   : availabilityError_;
    return false;
  }

  std::string failure;
  auto connection = systemBus(&failure);
  if (connection) {
    auto reply = call(connection.get(), dbusMethod(action),
                      g_variant_new("(b)", TRUE), G_VARIANT_TYPE("()"),
                      &failure);
    if (reply)
      return true;
  }

  if (error)
    *error = failure.empty() ? "The session action failed." : failure;
  refreshAvailability();
  return false;
}

} // namespace LunaDash
