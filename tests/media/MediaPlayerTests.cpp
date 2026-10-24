#include "core/filesystem/TemporaryDirectory.hpp"
#include "support/ProcessHarness.hpp"
#include "support/TestHarness.hpp"

#include <gio/gio.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace LunaDash {
namespace {
constexpr char kPlayerPath[] = "/org/mpris/MediaPlayer2";

// Answers every MPRIS call addressed to /org/mpris/MediaPlayer2 on one bus
// connection, like a minimal media player.
class PlayerFixture final {
public:
  std::string status = "Playing";
  std::string title = "Test track";
  double volume = 0.65;
  std::string lastMethod;
  std::int64_t position = 12000000;
  bool fail = false;
  std::mutex mutex;

  GVariant *properties(const std::string &interface) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    if (interface == "org.mpris.MediaPlayer2") {
      g_variant_builder_add(&builder, "{sv}", "Identity",
                            g_variant_new_string("Fixture player"));
      g_variant_builder_add(&builder, "{sv}", "CanRaise",
                            g_variant_new_boolean(TRUE));
    } else {
      GVariantBuilder metadata;
      g_variant_builder_init(&metadata, G_VARIANT_TYPE("a{sv}"));
      g_variant_builder_add(&metadata, "{sv}", "xesam:title",
                            g_variant_new_string(title.c_str()));
      const char *artists[] = {"Artist", nullptr};
      g_variant_builder_add(&metadata, "{sv}", "xesam:artist",
                            g_variant_new_strv(artists, -1));
      g_variant_builder_add(&metadata, "{sv}", "mpris:trackid",
                            g_variant_new_object_path("/tracks/test"));
      g_variant_builder_add(&metadata, "{sv}", "mpris:length",
                            g_variant_new_int64(180000000));
      g_variant_builder_add(
          &metadata, "{sv}", "xesam:lyrics",
          g_variant_new_string("[00:01.00]First line\n[00:12.00]Current line"));
      g_variant_builder_add(&builder, "{sv}", "PlaybackStatus",
                            g_variant_new_string(status.c_str()));
      g_variant_builder_add(&builder, "{sv}", "Metadata",
                            g_variant_builder_end(&metadata));
      g_variant_builder_add(&builder, "{sv}", "Position",
                            g_variant_new_int64(position));
      g_variant_builder_add(&builder, "{sv}", "Volume",
                            g_variant_new_double(volume));
      g_variant_builder_add(&builder, "{sv}", "Shuffle",
                            g_variant_new_boolean(FALSE));
      g_variant_builder_add(&builder, "{sv}", "LoopStatus",
                            g_variant_new_string("None"));
      for (const auto *name : {"CanControl", "CanPlay", "CanPause", "CanSeek",
                               "CanGoNext", "CanGoPrevious"})
        g_variant_builder_add(&builder, "{sv}", name,
                              g_variant_new_boolean(TRUE));
    }
    return g_variant_new("(a{sv})", &builder);
  }

  GDBusMessage *handle(GDBusMessage *message) {
    std::lock_guard lock(mutex);
    if (fail)
      return g_dbus_message_new_method_error(
          message, "org.freedesktop.DBus.Error.Failed", "Player unavailable");
    const std::string member = g_dbus_message_get_member(message);
    GVariant *body = g_dbus_message_get_body(message);
    GDBusMessage *reply = g_dbus_message_new_method_reply(message);
    if (member == "GetAll") {
      const gchar *interface = nullptr;
      g_variant_get(body, "(&s)", &interface);
      g_dbus_message_set_body(reply, properties(interface ? interface : ""));
    } else if (member == "Set") {
      const gchar *interface = nullptr;
      const gchar *property = nullptr;
      GVariant *value = nullptr;
      g_variant_get(body, "(&s&sv)", &interface, &property, &value);
      lastMethod = property ? property : "";
      if (lastMethod == "Volume" && value &&
          g_variant_is_of_type(value, G_VARIANT_TYPE_DOUBLE))
        volume = g_variant_get_double(value);
      if (value)
        g_variant_unref(value);
    } else {
      lastMethod = member;
      if (lastMethod == "PlayPause")
        status = status == "Playing" ? "Paused" : "Playing";
      if (lastMethod == "SetPosition" && body) {
        const gchar *track = nullptr;
        gint64 requested = 0;
        g_variant_get(body, "(&ox)", &track, &requested);
        position = requested;
      }
    }
    return reply;
  }
};

GDBusMessage *filterMessage(GDBusConnection *connection, GDBusMessage *message,
                            gboolean incoming, gpointer data) {
  if (!incoming ||
      g_dbus_message_get_message_type(message) !=
          G_DBUS_MESSAGE_TYPE_METHOD_CALL ||
      g_strcmp0(g_dbus_message_get_path(message), kPlayerPath) != 0)
    return message;
  auto *fixture = static_cast<PlayerFixture *>(data);
  GDBusMessage *reply = fixture->handle(message);
  g_dbus_connection_send_message(connection, reply,
                                 G_DBUS_SEND_MESSAGE_FLAGS_NONE, nullptr,
                                 nullptr);
  g_object_unref(reply);
  g_object_unref(message);
  return nullptr;
}

struct ConnectionDeleter {
  void operator()(GDBusConnection *connection) const {
    if (!connection)
      return;
    g_dbus_connection_close_sync(connection, nullptr, nullptr);
    g_object_unref(connection);
  }
};
using Connection = std::unique_ptr<GDBusConnection, ConnectionDeleter>;

Connection connectPlayer(const std::string &address, const char *service,
                         PlayerFixture &fixture) {
  GError *error = nullptr;
  Connection connection(g_dbus_connection_new_for_address_sync(
      address.c_str(),
      static_cast<GDBusConnectionFlags>(
          G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
          G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
      nullptr, nullptr, &error));
  if (error) {
    g_error_free(error);
    return {};
  }
  g_dbus_connection_add_filter(connection.get(), filterMessage, &fixture,
                               nullptr);
  GVariant *reply = g_dbus_connection_call_sync(
      connection.get(), "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "RequestName", g_variant_new("(su)", service, 4u),
      G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (error) {
    g_error_free(error);
    return {};
  }
  guint32 result = 0;
  g_variant_get(reply, "(u)", &result);
  g_variant_unref(reply);
  return result == 1 ? std::move(connection) : Connection{};
}
} // namespace

class MediaPlayerTests final {
  TemporaryDirectory runtime;
  Test::ChildProcess sessionDaemon;
  Test::ChildProcess userDaemon;
  std::string sessionAddress;
  Connection pausedBus;
  Connection playingBus;
  Connection userBus;
  PlayerFixture paused;
  PlayerFixture playing;
  PlayerFixture user;

  std::string startBus(Test::ChildProcess &process, const std::string &path) {
    if (!process.start("dbus-daemon",
                       {"--session", "--nofork", "--address=unix:path=" + path,
                        "--print-address=1"}))
      return {};
    return process.readLine(3000);
  }

  nlohmann::json invoke(const std::vector<std::string> &arguments,
                        int expected = 0, bool sameBus = false) {
    auto environment = Test::currentEnvironment();
    environment["DBUS_SESSION_BUS_ADDRESS"] =
        sameBus ? "unix:path=" + runtime.filePath("bus") : sessionAddress;
    environment["XDG_RUNTIME_DIR"] = runtime.path().string();
    const auto result = Test::runProcess(
        (Test::executableDirectory() / "lunadash-media-client").string(),
        arguments, environment, 6000);
    if (!result.started)
      FAIL("Could not start media helper");
    if (result.timedOut)
      FAIL("Media helper timed out");
    if (result.exitCode != expected)
      FAIL("Unexpected media helper exit " + std::to_string(result.exitCode) +
           ": " + result.standardError);
    return nlohmann::json::parse(result.standardOutput, nullptr, false);
  }

public:
  void initTestCase() {
    if (Test::findExecutable("dbus-daemon").empty())
      SKIP("dbus-daemon is unavailable");
    VERIFY(runtime.isValid());
    sessionAddress = startBus(sessionDaemon, runtime.filePath("private-bus"));
    const auto userAddress = startBus(userDaemon, runtime.filePath("bus"));
    VERIFY(!sessionAddress.empty());
    VERIFY(!userAddress.empty());
    paused.status = "Paused";
    paused.title = "Paused track";
    user.title = "User bus track";
    pausedBus = connectPlayer(sessionAddress, "org.mpris.MediaPlayer2.aaa", paused);
    playingBus =
        connectPlayer(sessionAddress, "org.mpris.MediaPlayer2.zzz", playing);
    userBus = connectPlayer(userAddress, "org.mpris.MediaPlayer2.sandbox", user);
    VERIFY(pausedBus);
    VERIFY(playingBus);
    VERIFY(userBus);
  }

  void discoversBothBusesAndPrioritizesPlaying() {
    const auto result = invoke({"media-status"});
    VERIFY(result.value("available", false));
    COMPARE(result.value("title", std::string()), std::string("Test track"));
    COMPARE(result.value("players", nlohmann::json::array()).size(),
            std::size_t{3});
    COMPARE(result.value("artist", std::string()), std::string("Artist"));
    COMPARE(result.value("trackId", std::string()), std::string("/tracks/test"));
    VERIFY(result.value("lyrics", std::string()).find("Current line") !=
           std::string::npos);
    COMPARE(invoke({"media-status"}, 0, true)
                .value("players", nlohmann::json::array())
                .size(),
            std::size_t{1});
  }

  void selectsAndControlsTheChosenPlayer() {
    const std::string service = "org.mpris.MediaPlayer2.sandbox";
    COMPARE(invoke({"media-status", service, "user"}).value("title", std::string()),
            std::string("User bus track"));
    auto result = invoke({"media-action", "play-pause", service, "user"});
    {
      std::lock_guard lock(user.mutex);
      COMPARE(user.status, std::string("Paused"));
    }
    VERIFY(!result.value("playing", true));
    invoke({"media-action", "volume", service, "user", "0.4"});
    {
      std::lock_guard lock(user.mutex);
      COMPARE(user.volume, 0.4);
    }
    invoke({"media-action", "seek", service, "user", "30000000", "/tracks/test"});
    {
      std::lock_guard lock(user.mutex);
      COMPARE(user.position, std::int64_t{30000000});
    }
    invoke({"media-action", "seek", service, "user", "40000000", "/tracks/stale"},
           2);
    {
      std::lock_guard lock(user.mutex);
      COMPARE(user.position, std::int64_t{30000000});
    }
    invoke({"media-action", "volume", service, "user", "nan"}, 2);
    {
      std::lock_guard lock(user.mutex);
      COMPARE(user.volume, 0.4);
    }
    invoke({"media-action", "raise", service, "user"});
    std::lock_guard lock(user.mutex);
    COMPARE(user.lastMethod, std::string("Raise"));
  }

  void staleOrBrokenPlayersDoNotBlockOthers() {
    {
      std::lock_guard lock(playing.mutex);
      playing.fail = true;
    }
    auto result = invoke({"media-status"});
    COMPARE(result.value("players", nlohmann::json::array()).size(),
            std::size_t{2});
    {
      std::lock_guard lock(paused.mutex);
      paused.lastMethod.clear();
    }
    result = invoke(
        {"media-action", "next", "org.mpris.MediaPlayer2.closed", "session"}, 2);
    VERIFY(result.contains("error"));
    {
      std::lock_guard lock(paused.mutex);
      VERIFY(paused.lastMethod.empty());
    }
    std::lock_guard lock(playing.mutex);
    playing.fail = false;
  }

  void cleanupTestCase() {
    pausedBus.reset();
    playingBus.reset();
    userBus.reset();
    sessionDaemon.terminate();
    userDaemon.terminate();
  }
};
} // namespace LunaDash

int main(int argc, char **argv) {
  using Fixture = LunaDash::MediaPlayerTests;
  return LunaDash::Test::run<Fixture>(
      "MediaPlayerTests",
      {{"initTestCase", &Fixture::initTestCase},
       {"discoversBothBusesAndPrioritizesPlaying",
        &Fixture::discoversBothBusesAndPrioritizesPlaying},
       {"selectsAndControlsTheChosenPlayer",
        &Fixture::selectsAndControlsTheChosenPlayer},
       {"staleOrBrokenPlayersDoNotBlockOthers",
        &Fixture::staleOrBrokenPlayersDoNotBlockOthers},
       {"cleanupTestCase", &Fixture::cleanupTestCase}},
      argc, argv);
}
