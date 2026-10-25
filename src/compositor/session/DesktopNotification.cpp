#include "compositor/session/DesktopNotification.hpp"

#include "config/localization/Localization.hpp"

#include <gio/gio.h>

namespace LunaDash {
namespace {
std::string tr(const std::string &text) {
  const auto dictionary = languageDictionary(selectedLanguage());
  const auto entry = dictionary.find(text);
  return entry != dictionary.end() && entry->is_string() ? entry->get<std::string>() : text;
}

struct PendingNotification {
  std::string summary;
  std::string body;
  std::string icon;
};

void busReady(GObject *, GAsyncResult *result, gpointer data) {
  auto *notification = static_cast<PendingNotification *>(data);
  GDBusConnection *connection = g_bus_get_finish(result, nullptr);
  if (connection) {
    GVariantBuilder actions;
    g_variant_builder_init(&actions, G_VARIANT_TYPE("as"));
    GVariantBuilder hints;
    g_variant_builder_init(&hints, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&hints, "{sv}", "desktop-entry",
                          g_variant_new_string("lunadash"));
    // No reply needed: a missing daemon simply means no notification.
    g_dbus_connection_call(
        connection, "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications", "Notify",
        g_variant_new("(susssasa{sv}i)", "LunaDash", 0U, notification->icon.c_str(),
                      notification->summary.c_str(), notification->body.c_str(),
                      &actions, &hints, -1),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START, 2000, nullptr, nullptr, nullptr);
    g_object_unref(connection);
  }
  delete notification;
}
} // namespace

void sendDesktopNotification(const std::string &summary, const std::string &body,
                             const std::string &icon) {
  auto *notification = new PendingNotification{tr(summary), body, icon};
  g_bus_get(G_BUS_TYPE_SESSION, nullptr, busReady, notification);
}

} // namespace LunaDash
