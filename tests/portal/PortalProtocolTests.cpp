#include "core/filesystem/TemporaryDirectory.hpp"
#include "support/ProcessHarness.hpp"
#include "support/TestHarness.hpp"

#include <gio/gio.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr char service[] = "org.freedesktop.impl.portal.desktop.lunadash";
constexpr char desktop[] = "/org/freedesktop/portal/desktop";
constexpr char fileInterface[] = "org.freedesktop.impl.portal.FileChooser";

struct VariantDeleter {
  void operator()(GVariant *value) const {
    if (value)
      g_variant_unref(value);
  }
};
using Variant = std::unique_ptr<GVariant, VariantDeleter>;

std::string errorText(GError *error) {
  std::string message = error ? error->message : "";
  if (error)
    g_error_free(error);
  return message;
}

// Concatenated argument signatures of one method or signal, or an empty
// string when the introspection XML does not describe it.
std::string signature(const std::string &xml, const std::string &interface,
                      const std::string &member, const std::string &direction) {
  GError *error = nullptr;
  GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(xml.c_str(), &error);
  if (!node) {
    errorText(error);
    return {};
  }
  std::string result;
  if (GDBusInterfaceInfo *info =
          g_dbus_node_info_lookup_interface(node, interface.c_str())) {
    if (GDBusMethodInfo *method =
            g_dbus_interface_info_lookup_method(info, member.c_str())) {
      for (GDBusArgInfo **argument =
               direction == "out" ? method->out_args : method->in_args;
           argument && *argument; ++argument)
        result += (*argument)->signature;
    } else if (GDBusSignalInfo *signal = g_dbus_interface_info_lookup_signal(
                   info, member.c_str())) {
      for (GDBusArgInfo **argument = signal->args; argument && *argument;
           ++argument)
        result += (*argument)->signature;
    }
  }
  g_dbus_node_info_unref(node);
  return result;
}

guint32 unwrapUnsigned(GVariant *value) {
  Variant current(g_variant_ref(value));
  while (current && g_variant_is_of_type(current.get(), G_VARIANT_TYPE_VARIANT))
    current.reset(g_variant_get_variant(current.get()));
  return current && g_variant_is_of_type(current.get(), G_VARIANT_TYPE_UINT32)
             ? g_variant_get_uint32(current.get())
             : 0;
}

struct PendingCall {
  bool finished = false;
  Variant reply;
  std::string error;
};

void callFinished(GObject *source, GAsyncResult *result, gpointer data) {
  auto *pending = static_cast<PendingCall *>(data);
  GError *error = nullptr;
  pending->reply.reset(g_dbus_connection_call_finish(
      G_DBUS_CONNECTION(source), result, &error));
  pending->error = errorText(error);
  pending->finished = true;
}
} // namespace

class PortalProtocolTests final {
  LunaDash::TemporaryDirectory directory_;
  LunaDash::Test::ChildProcess backend_;
  GDBusConnection *bus_ = nullptr;

  Variant call(const std::string &path, const std::string &interface,
               const std::string &method, GVariant *arguments = nullptr,
               std::string *error = nullptr) {
    GError *failure = nullptr;
    Variant reply(g_dbus_connection_call_sync(
        bus_, service, path.c_str(), interface.c_str(), method.c_str(),
        arguments, nullptr, G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, &failure));
    const auto message = errorText(failure);
    if (error)
      *error = message;
    return reply;
  }

  std::string introspect(const std::string &path) {
    const auto reply =
        call(path, "org.freedesktop.DBus.Introspectable", "Introspect");
    if (!reply)
      return {};
    const gchar *xml = nullptr;
    g_variant_get(reply.get(), "(&s)", &xml);
    return xml ? xml : "";
  }

  bool serviceRegistered() {
    GError *error = nullptr;
    Variant reply(g_dbus_connection_call_sync(
        bus_, "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "NameHasOwner", g_variant_new("(s)", service),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, &error));
    errorText(error);
    gboolean owned = FALSE;
    if (reply)
      g_variant_get(reply.get(), "(b)", &owned);
    return owned;
  }

  void pump() { g_main_context_iteration(nullptr, FALSE); }

public:
  void initTestCase() {
    GError *error = nullptr;
    bus_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    const auto message = errorText(error);
    if (!bus_)
      SKIP("A D-Bus session bus is unavailable: " + message);
  }

  void cleanupTestCase() {
    if (bus_)
      g_object_unref(bus_);
    bus_ = nullptr;
  }

  void init() {
    VERIFY(directory_.isValid());
    VERIFY(bus_ != nullptr);
    auto environment = LunaDash::Test::currentEnvironment();
    environment["GTK_USE_PORTAL"] = "1";
    environment["XDG_CURRENT_DESKTOP"] = "LunaDash";
    environment["XDG_CONFIG_HOME"] = directory_.filePath("config");
    environment["XDG_DATA_HOME"] = directory_.filePath("data");
    environment["XDG_CACHE_HOME"] = directory_.filePath("cache");
    environment["XDG_RUNTIME_DIR"] = directory_.path().string();
    environment["LUNADASH_FILE_PICKER_TEST_HOLD"] = "1";
    VERIFY(backend_.start(
        (LunaDash::Test::executableDirectory() / "xdg-desktop-portal-lunadash")
            .string(),
        {}, environment));
    TRY_VERIFY_WITH_TIMEOUT(serviceRegistered(), 8000);
  }

  void cleanup() {
    backend_.terminate();
    if (bus_)
      TRY_VERIFY_WITH_TIMEOUT(!serviceRegistered(), 3000);
  }

  void coldStartExportsCompleteInterfaces() {
    // No direct C++ calls or warm-up requests may register the missing types.
    const auto xml = introspect(desktop);
    VERIFY2(!xml.empty(), "The first introspection request must succeed");
    for (const auto *method : {"OpenFile", "SaveFile", "SaveFiles"}) {
      COMPARE(signature(xml, fileInterface, method, "in"),
              std::string("osssa{sv}"));
      COMPARE(signature(xml, fileInterface, method, "out"),
              std::string("ua{sv}"));
    }
    COMPARE(signature(xml, "org.freedesktop.impl.portal.Settings",
                      "SettingChanged", {}),
            std::string("ssv"));
    std::string error;
    const auto version =
        call(desktop, "org.freedesktop.DBus.Properties", "Get",
             g_variant_new("(ss)", fileInterface, "version"), &error);
    VERIFY2(version != nullptr, error);
    COMPARE(unwrapUnsigned(version.get()), 4u);
    const auto scheme =
        call(desktop, "org.freedesktop.impl.portal.Settings", "Read",
             g_variant_new("(ss)", "org.freedesktop.appearance", "color-scheme"),
             &error);
    VERIFY2(scheme != nullptr, error);
    const auto colorScheme = unwrapUnsigned(scheme.get());
    VERIFY(colorScheme == 1u || colorScheme == 2u);
    const auto settingsVersion = call(
        desktop, "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", "org.freedesktop.impl.portal.Settings", "version"),
        &error);
    VERIFY2(settingsVersion != nullptr, error);
    COMPARE(unwrapUnsigned(settingsVersion.get()), 2u);
    const char *noNamespaces[] = {nullptr};
    const auto readAll =
        call(desktop, "org.freedesktop.impl.portal.Settings", "ReadAll",
             g_variant_new("(^as)", noNamespaces), &error);
    VERIFY2(readAll != nullptr, error);
  }

  void closeCancelsAcrossProcesses() {
    for (const auto *method : {"OpenFile", "SaveFile", "SaveFiles"}) {
      if (std::string(method) != "OpenFile") {
        cleanup();
        init();
      }
      closeCancelsFor(method);
    }
  }

  void closeCancelsFor(const std::string &method) {
    const std::string path = std::string(desktop) + "/request/ctest/cancel";
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&options, "{sv}", "current_folder",
                          g_variant_new_bytestring(directory_.path().c_str()));
    if (method == "SaveFiles") {
      GVariantBuilder files;
      g_variant_builder_init(&files, G_VARIANT_TYPE("aay"));
      for (const std::string name : {"draft.txt", "archive.tar.gz"})
        g_variant_builder_add_value(
            &files, g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, name.data(),
                                              name.size(), 1));
      g_variant_builder_add(&options, "{sv}", "files",
                            g_variant_builder_end(&files));
    }
    PendingCall pending;
    g_dbus_connection_call(
        bus_, service, desktop, fileInterface, method.c_str(),
        g_variant_new("(osssa{sv})", path.c_str(), "", "",
                      "Portal cancellation regression", &options),
        G_VARIANT_TYPE("(ua{sv})"), G_DBUS_CALL_FLAGS_NONE, 10000, nullptr,
        callFinished, &pending);
    VERIFY(LunaDash::Test::waitUntil(
        [&] {
          return introspect(path).find("org.freedesktop.impl.portal.Request") !=
                 std::string::npos;
        },
        5000, [this] { pump(); }));
    pump();
    VERIFY(!pending.finished);
    std::string error;
    const auto closed =
        call(path, "org.freedesktop.impl.portal.Request", "Close", nullptr, &error);
    VERIFY2(closed != nullptr, error);
    VERIFY(LunaDash::Test::waitUntil([&] { return pending.finished; }, 3000,
                                     [this] { pump(); }));
    VERIFY2(pending.reply != nullptr, pending.error);
    guint32 response = 0;
    GVariant *results = nullptr;
    g_variant_get(pending.reply.get(), "(u@a{sv})", &response, &results);
    COMPARE(response, 1u);
    VERIFY(g_variant_n_children(results) == 0);
    g_variant_unref(results);
    VERIFY(backend_.running());
    VERIFY(introspect(desktop).find(fileInterface) != std::string::npos);
  }
};

int main(int argc, char **argv) {
  return LunaDash::Test::run<PortalProtocolTests>(
      "PortalProtocolTests",
      {{"initTestCase", &PortalProtocolTests::initTestCase},
       {"cleanupTestCase", &PortalProtocolTests::cleanupTestCase},
       {"init", &PortalProtocolTests::init},
       {"cleanup", &PortalProtocolTests::cleanup},
       {"coldStartExportsCompleteInterfaces",
        &PortalProtocolTests::coldStartExportsCompleteInterfaces},
       {"closeCancelsAcrossProcesses",
        &PortalProtocolTests::closeCancelsAcrossProcesses}},
      argc, argv);
}
