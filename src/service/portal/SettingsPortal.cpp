#include "service/portal/SettingsPortal.hpp"

#include "config/appearance/AppearancePalette.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"

#include <algorithm>
#include <filesystem>
#include <string_view>

namespace LunaDash {
namespace {

constexpr char kDesktopPath[] = "/org/freedesktop/portal/desktop";
constexpr char kSettingsInterface[] = "org.freedesktop.impl.portal.Settings";
constexpr char kAppearanceNamespace[] = "org.freedesktop.appearance";

bool namespaceMatches(const gchar *filter) {
  if (!filter || !*filter)
    return true;
  const std::string_view requested(filter);
  if (requested.ends_with('*'))
    return std::string_view(kAppearanceNamespace)
        .starts_with(requested.substr(0, requested.size() - 1));
  return requested == kAppearanceNamespace;
}

const GDBusInterfaceVTable &settingsVTable() {
  static const GDBusInterfaceVTable table{
      SettingsPortal::methodCall, SettingsPortal::getProperty, nullptr,
      {nullptr}};
  return table;
}

} // namespace

SettingsPortal::SettingsPortal(GDBusConnection *connection,
                               GDBusNodeInfo *introspection)
    : connection_(G_DBUS_CONNECTION(g_object_ref(connection))),
      introspection_(g_dbus_node_info_ref(introspection)) {
  watchSettings();
  refresh();
  refreshTimer_ = g_timeout_add_seconds(30, [](gpointer data) -> gboolean {
    static_cast<SettingsPortal *>(data)->refresh();
    static_cast<SettingsPortal *>(data)->watchSettings();
    return G_SOURCE_CONTINUE;
  }, this);
}

SettingsPortal::~SettingsPortal() {
  if (debounce_)
    g_source_remove(debounce_);
  if (refreshTimer_)
    g_source_remove(refreshTimer_);
  if (registration_)
    g_dbus_connection_unregister_object(connection_, registration_);
  if (monitor_)
    g_object_unref(monitor_);
  g_dbus_node_info_unref(introspection_);
  g_object_unref(connection_);
}

bool SettingsPortal::registerObject(GError **error) {
  GDBusInterfaceInfo *interface =
      g_dbus_node_info_lookup_interface(introspection_, kSettingsInterface);
  registration_ = g_dbus_connection_register_object(
      connection_, kDesktopPath, interface, &settingsVTable(), this, nullptr,
      error);
  return registration_ != 0;
}

void SettingsPortal::watchSettings() {
  if (monitor_)
    return;
  const auto path = std::filesystem::path(g_get_user_config_dir()) / "LunaDash";
  GFile *directory = g_file_new_for_path(path.c_str());
  GError *error = nullptr;
  monitor_ = g_file_monitor_directory(directory, G_FILE_MONITOR_NONE, nullptr,
                                      &error);
  g_object_unref(directory);
  if (!monitor_) {
    g_clear_error(&error);
    return;
  }
  g_signal_connect(
      monitor_, "changed",
      G_CALLBACK(+[](GFileMonitor *, GFile *file, GFile *,
                     GFileMonitorEvent, gpointer data) {
        if (!file)
          return;
        gchar *basename = g_file_get_basename(file);
        const bool relevant =
            g_strcmp0(basename, "desktop-preferences.json") == 0;
        g_free(basename);
        if (relevant)
          static_cast<SettingsPortal *>(data)->scheduleRefresh();
      }),
      this);
}

void SettingsPortal::scheduleRefresh() {
  if (debounce_)
    g_source_remove(debounce_);
  debounce_ = g_timeout_add(100, [](gpointer data) -> gboolean {
    auto *portal = static_cast<SettingsPortal *>(data);
    portal->debounce_ = 0;
    portal->refresh();
    portal->watchSettings();
    return G_SOURCE_REMOVE;
  }, this);
}

void SettingsPortal::refresh() {
  const auto preferences = loadDesktopPreferences();
  const auto animations = preferences.find("animations");
  const bool motion = animations == preferences.end() ||
                      !animations->is_boolean() ||
                      animations->get<bool>();
  const std::map<std::string, std::uint32_t> next{
      {"color-scheme", appearanceIsDark(preferences) ? 1u : 2u},
      {"contrast", 0u},
      {"reduced-motion", motion ? 0u : 1u}};
  for (const auto &[key, value] : next) {
    const auto previous = values_.find(key);
    if (previous != values_.end() && previous->second == value)
      continue;
    GError *error = nullptr;
    g_dbus_connection_emit_signal(
        connection_, nullptr, kDesktopPath, kSettingsInterface,
        "SettingChanged",
        g_variant_new("(ssv)", kAppearanceNamespace, key.c_str(),
                      g_variant_new_uint32(value)),
        &error);
    g_clear_error(&error);
  }
  values_ = next;
}

void SettingsPortal::methodCall(GDBusConnection *, const gchar *,
                                const gchar *, const gchar *,
                                const gchar *method, GVariant *parameters,
                                GDBusMethodInvocation *invocation,
                                gpointer userData) {
  auto *self = static_cast<SettingsPortal *>(userData);
  self->refresh();
  if (g_strcmp0(method, "Read") == 0) {
    const gchar *nameSpace = nullptr;
    const gchar *key = nullptr;
    g_variant_get(parameters, "(&s&s)", &nameSpace, &key);
    if (g_strcmp0(nameSpace, kAppearanceNamespace) == 0) {
      const auto found = self->values_.find(key ? key : "");
      if (found != self->values_.end()) {
        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(v)", g_variant_new_uint32(found->second)));
        return;
      }
    }
    g_dbus_method_invocation_return_dbus_error(
        invocation, "org.freedesktop.portal.Error.NotFound",
        "Unknown desktop setting.");
    return;
  }
  if (g_strcmp0(method, "ReadAll") == 0) {
    GVariant *namespaces = g_variant_get_child_value(parameters, 0);
    bool includeAppearance = g_variant_n_children(namespaces) == 0;
    GVariantIter iterator;
    g_variant_iter_init(&iterator, namespaces);
    const gchar *filter = nullptr;
    while (!includeAppearance &&
           g_variant_iter_next(&iterator, "&s", &filter))
      includeAppearance = namespaceMatches(filter);
    g_variant_unref(namespaces);

    GVariantBuilder all;
    g_variant_builder_init(&all, G_VARIANT_TYPE("a{sa{sv}}"));
    if (includeAppearance) {
      GVariantBuilder settings;
      g_variant_builder_init(&settings, G_VARIANT_TYPE_VARDICT);
      for (const auto &[key, value] : self->values_)
        g_variant_builder_add(&settings, "{sv}", key.c_str(),
                              g_variant_new_uint32(value));
      g_variant_builder_add(&all, "{s@a{sv}}", kAppearanceNamespace,
                            g_variant_builder_end(&settings));
    }
    g_dbus_method_invocation_return_value(
        invocation, g_variant_new("(@a{sa{sv}})",
                                  g_variant_builder_end(&all)));
    return;
  }
  g_dbus_method_invocation_return_error_literal(
      invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
      "Unknown Settings method.");
}

GVariant *SettingsPortal::getProperty(GDBusConnection *, const gchar *,
                                      const gchar *, const gchar *,
                                      const gchar *property, GError **error,
                                      gpointer) {
  if (g_strcmp0(property, "version") == 0)
    return g_variant_new_uint32(2);
  g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
              "Unknown Settings property '%s'.", property);
  return nullptr;
}

} // namespace LunaDash
