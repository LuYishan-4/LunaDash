#include "service/portal/Portal.hpp"

#include "service/portal/SettingsPortal.hpp"

namespace LunaDash {
namespace {

constexpr char kPortalService[] =
    "org.freedesktop.impl.portal.desktop.lunadash";
constexpr char kPortalXml[] = R"xml(
<node>
  <interface name="org.freedesktop.impl.portal.Settings">
    <method name="Read">
      <arg name="namespace" type="s" direction="in"/>
      <arg name="key" type="s" direction="in"/>
      <arg name="value" type="v" direction="out"/>
    </method>
    <method name="ReadAll">
      <arg name="namespaces" type="as" direction="in"/>
      <arg name="values" type="a{sa{sv}}" direction="out"/>
    </method>
    <signal name="SettingChanged">
      <arg name="namespace" type="s"/>
      <arg name="key" type="s"/>
      <arg name="value" type="v"/>
    </signal>
    <property name="version" type="u" access="read"/>
  </interface>
</node>
)xml";

struct NameState {
  GMainLoop *loop = nullptr;
  bool acquired = false;
};

void nameAcquired(GDBusConnection *, const gchar *, gpointer userData) {
  auto *state = static_cast<NameState *>(userData);
  state->acquired = true;
  g_message("LunaDash portal ready: Settings v2");
}

void nameLost(GDBusConnection *, const gchar *, gpointer userData) {
  auto *state = static_cast<NameState *>(userData);
  if (!state->acquired)
    g_critical("LunaDash portal could not own its D-Bus service name.");
  if (state->loop)
    g_main_loop_quit(state->loop);
}

} // namespace

int runPortal(int argc, char **argv) {
  return Portal{}.run(argc, argv);
}

int Portal::run(int argc, char **argv) {
  (void)argc;
  (void)argv;
  GError *error = nullptr;
  GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr,
                                                &error);
  if (!connection) {
    g_critical("LunaDash portal: session bus unavailable: %s",
               error ? error->message : "unknown error");
    g_clear_error(&error);
    return 2;
  }
  GDBusNodeInfo *introspection =
      g_dbus_node_info_new_for_xml(kPortalXml, &error);
  if (!introspection) {
    g_critical("LunaDash portal: invalid interface description: %s",
               error ? error->message : "unknown error");
    g_clear_error(&error);
    g_object_unref(connection);
    return 4;
  }

  SettingsPortal settings(connection, introspection);
  if (!settings.registerObject(&error)) {
    g_critical("LunaDash portal: cannot export interfaces: %s",
               error ? error->message : "unknown error");
    g_clear_error(&error);
    g_dbus_node_info_unref(introspection);
    g_object_unref(connection);
    return 4;
  }

  GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
  NameState state{loop, false};
  const guint owner = g_bus_own_name_on_connection(
      connection, kPortalService, G_BUS_NAME_OWNER_FLAGS_NONE, nameAcquired,
      nameLost, &state, nullptr);
  g_main_loop_run(loop);
  g_bus_unown_name(owner);
  g_main_loop_unref(loop);
  g_dbus_node_info_unref(introspection);
  g_object_unref(connection);
  return 3;
}

} // namespace LunaDash
