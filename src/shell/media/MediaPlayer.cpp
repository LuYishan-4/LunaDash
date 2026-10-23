#include "shell/media/MediaPlayer.hpp"

#include <gio/gio.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <unistd.h>

namespace LunaDash {
namespace {
constexpr std::string_view kPrefix = "org.mpris.MediaPlayer2.";
constexpr char kPath[] = "/org/mpris/MediaPlayer2";
constexpr char kRoot[] = "org.mpris.MediaPlayer2";
constexpr char kPlayer[] = "org.mpris.MediaPlayer2.Player";
constexpr char kProperties[] = "org.freedesktop.DBus.Properties";
constexpr int kTimeoutMilliseconds = 700;

struct VariantDeleter {
  void operator()(GVariant *value) const {
    if (value)
      g_variant_unref(value);
  }
};
using Variant = std::unique_ptr<GVariant, VariantDeleter>;
using PropertyMap = std::map<std::string, Variant, std::less<>>;
using Connection = std::shared_ptr<GDBusConnection>;

struct Bus {
  std::string name;
  Connection connection;
};

struct Player {
  std::string service;
  std::string bus;
  PropertyMap root;
  PropertyMap properties;
};

struct CollectState {
  GMainLoop *loop = nullptr;
  std::size_t remaining = 0;
  std::vector<std::shared_ptr<Player>> players;

  ~CollectState() {
    if (loop)
      g_main_loop_unref(loop);
  }
};

struct PendingReply {
  std::shared_ptr<CollectState> state;
  std::shared_ptr<Player> player;
  bool root = false;
};

Connection ownConnection(GDBusConnection *connection) {
  if (!connection)
    return {};
  g_dbus_connection_set_exit_on_close(connection, FALSE);
  return Connection(connection, [](GDBusConnection *value) {
    g_object_unref(value);
  });
}

Variant callSync(GDBusConnection *connection, const char *destination,
                 const char *path, const char *interface,
                 const char *method, GVariant *arguments,
                 std::string *errorMessage = nullptr) {
  if (!connection) {
    if (errorMessage)
      *errorMessage = "The media connection is unavailable.";
    return {};
  }
  GError *error = nullptr;
  GVariant *reply = g_dbus_connection_call_sync(
      connection, destination, path, interface, method, arguments, nullptr,
      G_DBUS_CALL_FLAGS_NONE, kTimeoutMilliseconds, nullptr, &error);
  if (error) {
    if (errorMessage)
      *errorMessage = error->message ? error->message : "D-Bus call failed.";
    g_error_free(error);
  }
  return Variant(reply);
}

PropertyMap readProperties(GVariant *dictionary) {
  PropertyMap result;
  if (!dictionary || !g_variant_is_of_type(dictionary, G_VARIANT_TYPE("a{sv}")))
    return result;

  GVariantIter iterator;
  g_variant_iter_init(&iterator, dictionary);
  GVariant *entry = nullptr;
  while ((entry = g_variant_iter_next_value(&iterator))) {
    Variant ownedEntry(entry);
    Variant key(g_variant_get_child_value(entry, 0));
    Variant boxedValue(g_variant_get_child_value(entry, 1));
    if (!key || !boxedValue)
      continue;
    const char *name = g_variant_get_string(key.get(), nullptr);
    GVariant *value = g_variant_is_of_type(boxedValue.get(),
                                            G_VARIANT_TYPE_VARIANT)
                          ? g_variant_get_variant(boxedValue.get())
                          : g_variant_ref(boxedValue.get());
    if (name && value)
      result.emplace(name, Variant(value));
    else if (value)
      g_variant_unref(value);
  }
  return result;
}

std::string stringValue(const PropertyMap &properties, std::string_view key,
                        std::string fallback = {}) {
  const auto found = properties.find(key);
  if (found == properties.end() || !found->second)
    return fallback;
  GVariant *value = found->second.get();
  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING) &&
      !g_variant_is_of_type(value, G_VARIANT_TYPE_OBJECT_PATH))
    return fallback;
  return g_variant_get_string(value, nullptr);
}

bool boolValue(const PropertyMap &properties, std::string_view key,
               bool fallback = false) {
  const auto found = properties.find(key);
  if (found == properties.end() || !found->second ||
      !g_variant_is_of_type(found->second.get(), G_VARIANT_TYPE_BOOLEAN))
    return fallback;
  return g_variant_get_boolean(found->second.get());
}

double doubleValue(const PropertyMap &properties, std::string_view key,
                   double fallback = 0.0) {
  const auto found = properties.find(key);
  if (found == properties.end() || !found->second)
    return fallback;
  GVariant *value = found->second.get();
  if (g_variant_is_of_type(value, G_VARIANT_TYPE_DOUBLE))
    return g_variant_get_double(value);
  if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64))
    return static_cast<double>(g_variant_get_int64(value));
  if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64))
    return static_cast<double>(g_variant_get_uint64(value));
  return fallback;
}

int64_t integerValue(const PropertyMap &properties, std::string_view key,
                     int64_t fallback = 0) {
  const auto found = properties.find(key);
  if (found == properties.end() || !found->second)
    return fallback;
  GVariant *value = found->second.get();
  if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64))
    return g_variant_get_int64(value);
  if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64))
    return static_cast<int64_t>(g_variant_get_uint64(value));
  if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32))
    return g_variant_get_int32(value);
  if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
    return g_variant_get_uint32(value);
  return fallback;
}

std::string stringList(const PropertyMap &properties, std::string_view key) {
  const auto found = properties.find(key);
  if (found == properties.end() || !found->second)
    return {};
  GVariant *value = found->second.get();
  if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING))
    return g_variant_get_string(value, nullptr);
  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING_ARRAY))
    return {};

  std::string result;
  const gsize length = g_variant_n_children(value);
  for (gsize index = 0; index < length; ++index) {
    Variant item(g_variant_get_child_value(value, index));
    if (!item)
      continue;
    if (!result.empty())
      result += ", ";
    result += g_variant_get_string(item.get(), nullptr);
  }
  return result;
}

std::string makeValidUtf8(const std::string &value, std::size_t maximumCharacters) {
  char *valid = g_utf8_make_valid(value.data(),
                                  static_cast<gssize>(value.size()));
  if (!valid)
    return {};
  std::string result(valid);
  g_free(valid);
  const glong characters = g_utf8_strlen(result.c_str(), -1);
  if (characters > static_cast<glong>(maximumCharacters)) {
    const char *end = g_utf8_offset_to_pointer(
        result.c_str(), static_cast<glong>(maximumCharacters));
    result.resize(static_cast<std::size_t>(end - result.c_str()));
  }
  return result;
}

std::string busId(GDBusConnection *connection) {
  const auto reply = callSync(connection, "org.freedesktop.DBus",
                              "/org/freedesktop/DBus", "org.freedesktop.DBus",
                              "GetId", nullptr);
  if (!reply || !g_variant_is_of_type(reply.get(), G_VARIANT_TYPE("(s)")))
    return {};
  const char *id = nullptr;
  g_variant_get(reply.get(), "(&s)", &id);
  return id ? id : "";
}

std::vector<Bus> mediaBuses() {
  std::vector<Bus> result;
  std::string sessionId;
  GError *error = nullptr;
  auto session = ownConnection(
      g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error));
  if (error)
    g_error_free(error);
  if (session) {
    result.push_back({"session", session});
    sessionId = busId(session.get());
  }

  const char *runtimeDirectory = std::getenv("XDG_RUNTIME_DIR");
  const std::filesystem::path runtime =
      runtimeDirectory && *runtimeDirectory
          ? std::filesystem::path(runtimeDirectory)
          : std::filesystem::path("/run/user") /
                std::to_string(static_cast<unsigned long>(::getuid()));
  const auto socket = runtime / "bus";
  std::error_code filesystemError;
  if (!std::filesystem::exists(socket, filesystemError))
    return result;

  char *escapedPath = g_dbus_address_escape_value(socket.c_str());
  if (!escapedPath)
    return result;
  const std::string address = std::string("unix:path=") + escapedPath;
  g_free(escapedPath);
  error = nullptr;
  auto user = ownConnection(g_dbus_connection_new_for_address_sync(
      address.c_str(),
      static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                        G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
      nullptr, nullptr, &error));
  if (error)
    g_error_free(error);
  if (user && (sessionId.empty() || busId(user.get()) != sessionId))
    result.push_back({"user", std::move(user)});
  return result;
}

void getAllFinished(GObject *source, GAsyncResult *result, gpointer data) {
  std::unique_ptr<PendingReply> pending(static_cast<PendingReply *>(data));
  GError *error = nullptr;
  Variant reply(g_dbus_connection_call_finish(
      G_DBUS_CONNECTION(source), result, &error));
  if (error)
    g_error_free(error);
  if (reply && g_variant_n_children(reply.get()) > 0) {
    Variant dictionary(g_variant_get_child_value(reply.get(), 0));
    auto values = readProperties(dictionary.get());
    if (pending->root)
      pending->player->root = std::move(values);
    else
      pending->player->properties = std::move(values);
  }
  if (pending->state->remaining > 0 && --pending->state->remaining == 0)
    g_main_loop_quit(pending->state->loop);
}

std::vector<std::string> listNames(GDBusConnection *connection) {
  std::vector<std::string> result;
  const auto reply = callSync(connection, "org.freedesktop.DBus",
                              "/org/freedesktop/DBus", "org.freedesktop.DBus",
                              "ListNames", nullptr);
  if (!reply || g_variant_n_children(reply.get()) == 0)
    return result;
  Variant names(g_variant_get_child_value(reply.get(), 0));
  if (!names || !g_variant_is_of_type(names.get(), G_VARIANT_TYPE_STRING_ARRAY))
    return result;
  const gsize length = g_variant_n_children(names.get());
  result.reserve(length);
  for (gsize index = 0; index < length; ++index) {
    Variant name(g_variant_get_child_value(names.get(), index));
    if (name)
      result.emplace_back(g_variant_get_string(name.get(), nullptr));
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<std::shared_ptr<Player>>
collect(const std::vector<Bus> &buses) {
  auto state = std::make_shared<CollectState>();
  state->loop = g_main_loop_new(nullptr, FALSE);
  for (const auto &bus : buses) {
    for (const auto &name : listNames(bus.connection.get())) {
      if (!name.starts_with(kPrefix))
        continue;
      auto player = std::make_shared<Player>(Player{name, bus.name, {}, {}});
      state->players.push_back(player);
      for (const bool root : {true, false}) {
        ++state->remaining;
        auto *pending = new PendingReply{state, player, root};
        g_dbus_connection_call(
            bus.connection.get(), name.c_str(), kPath, kProperties, "GetAll",
            g_variant_new("(s)", root ? kRoot : kPlayer),
            G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE,
            kTimeoutMilliseconds, nullptr, getAllFinished, pending);
      }
    }
  }
  if (state->remaining > 0)
    g_main_loop_run(state->loop);
  return std::move(state->players);
}

std::string lyrics(const PropertyMap &metadata) {
  for (const auto key : {"xesam:lyrics", "lyrics", "xesam:asText"}) {
    auto value = stringValue(metadata, key);
    if (!value.empty()) {
      return makeValidUtf8(value, 131072);
    }
  }

  const std::string url = stringValue(metadata, "xesam:url");
  if (url.empty())
    return {};
  GError *error = nullptr;
  char *filename = g_filename_from_uri(url.c_str(), nullptr, &error);
  if (error)
    g_error_free(error);
  if (!filename)
    return {};

  std::filesystem::path lyricsPath(filename);
  g_free(filename);
  lyricsPath.replace_extension(".lrc");
  std::error_code filesystemError;
  const auto size = std::filesystem::file_size(lyricsPath, filesystemError);
  if (filesystemError || size > 262144)
    return {};
  std::ifstream file(lyricsPath, std::ios::binary);
  if (!file)
    return {};
  const std::string content{std::istreambuf_iterator<char>(file),
                            std::istreambuf_iterator<char>()};
  return makeValidUtf8(content, 262144);
}

nlohmann::json snapshot(const Player &player) {
  const auto &properties = player.properties;
  const auto playback = properties.find("PlaybackStatus");
  if (playback == properties.end())
    return {};
  const std::string status = stringValue(properties, "PlaybackStatus");
  const auto metadataValue = properties.find("Metadata");
  PropertyMap metadata;
  if (metadataValue != properties.end() && metadataValue->second)
    metadata = readProperties(metadataValue->second.get());
  const bool canControl = boolValue(properties, "CanControl", true);
  std::string identity = stringValue(player.root, "Identity");
  if (identity.empty())
    identity = player.service.substr(kPrefix.size());

  nlohmann::json result = {
      {"available", true},
      {"service", player.service},
      {"bus", player.bus},
      {"identity", identity},
      {"desktopEntry", stringValue(player.root, "DesktopEntry")},
      {"playbackStatus", status},
      {"playing", status == "Playing"},
      {"title", stringValue(metadata, "xesam:title")},
      {"artist", stringList(metadata, "xesam:artist")},
      {"album", stringValue(metadata, "xesam:album")},
      {"artUrl", stringValue(metadata, "mpris:artUrl")},
      {"trackUrl", stringValue(metadata, "xesam:url")},
      {"trackId", stringValue(metadata, "mpris:trackid")},
      {"lengthUs", integerValue(metadata, "mpris:length")},
      {"positionUs", integerValue(properties, "Position")},
      {"positionSupported", properties.contains("Position")},
      {"rate", doubleValue(properties, "Rate", 1.0)},
      {"volume", doubleValue(properties, "Volume")},
      {"volumeSupported", canControl && properties.contains("Volume")},
      {"shuffle", boolValue(properties, "Shuffle")},
      {"shuffleSupported", canControl && properties.contains("Shuffle")},
      {"loopStatus", stringValue(properties, "LoopStatus", "None")},
      {"loopSupported", canControl && properties.contains("LoopStatus")},
      {"canRaise", boolValue(player.root, "CanRaise")},
      {"lyrics", lyrics(metadata)}};
  for (const auto capability : {"Play", "Pause", "GoNext", "GoPrevious",
                                "Seek"})
    result[std::string("can") + capability] =
        canControl && boolValue(properties, std::string("Can") + capability);
  return result;
}

nlohmann::json select(const std::vector<std::shared_ptr<Player>> &players,
                      const std::string &service, const std::string &bus) {
  nlohmann::json list = nlohmann::json::array();
  std::vector<nlohmann::json> snapshots;
  nlohmann::json selected;
  for (const auto &player : players) {
    auto data = snapshot(*player);
    if (!data.is_object() || data.empty())
      continue;
    snapshots.push_back(data);
    list.push_back({{"service", data["service"]},
                    {"bus", data["bus"]},
                    {"identity", data["identity"]},
                    {"playing", data["playing"]}});
    if (data["service"] == service && (bus.empty() || data["bus"] == bus))
      selected = std::move(data);
  }
  if (selected.empty())
    for (const auto &data : snapshots) {
      if (selected.empty() ||
          (!selected.value("playing", false) && data.value("playing", false)))
        selected = data;
    }
  if (selected.empty())
    selected = {{"available", false}};
  selected["players"] = std::move(list);
  return selected;
}

} // namespace

nlohmann::json mediaStatus(const std::string &preferredService,
                          const std::string &preferredBus) {
  const auto buses = mediaBuses();
  auto players = collect(buses);
  return select(players, preferredService, preferredBus);
}

nlohmann::json mediaAction(const std::string &action, const std::string &service,
                           const std::string &busName, const std::string &value,
                           const std::string &trackId) {
  const auto buses = mediaBuses();
  auto players = collect(buses);
  auto current = select(players, service, busName);
  const auto fail = [&](const std::string &message) {
    auto result = current;
    result["error"] = message;
    return result;
  };
  if (!current.value("available", false) ||
      (!service.empty() &&
       (current.value("service", std::string{}) != service ||
        (!busName.empty() && current.value("bus", std::string{}) != busName))))
    return fail("The selected media player is no longer available.");

  const auto bus = std::find_if(buses.begin(), buses.end(), [&](const Bus &entry) {
    return entry.name == current.value("bus", std::string{});
  });
  if (bus == buses.end())
    return fail("The media connection is unavailable.");
  const std::string selected = current.value("service", std::string{});
  std::string interface = kPlayer;
  std::string method;
  std::string error;
  Variant reply;

  static const std::map<std::string, std::string, std::less<>> methods{
      {"play-pause", "PlayPause"}, {"play", "Play"},
      {"pause", "Pause"},          {"next", "Next"},
      {"previous", "Previous"},    {"raise", "Raise"}};
  if (const auto found = methods.find(action); found != methods.end()) {
    method = found->second;
    if (action == "raise")
      interface = kRoot;
    reply = callSync(bus->connection.get(), selected.c_str(), kPath,
                     interface.c_str(), method.c_str(), nullptr, &error);
  } else if (action == "volume" || action == "shuffle" || action == "loop") {
    std::string property;
    GVariant *content = nullptr;
    if (action == "volume") {
      char *end = nullptr;
      const double volume = std::strtod(value.c_str(), &end);
      if (end == value.c_str() || *end != '\0' || !std::isfinite(volume) ||
          volume < 0 || volume > 1)
        return fail("Invalid media volume.");
      property = "Volume";
      content = g_variant_new_double(volume);
    } else if (action == "shuffle") {
      if (value != "true" && value != "false")
        return fail("Invalid shuffle state.");
      property = "Shuffle";
      content = g_variant_new_boolean(value == "true");
    } else {
      if (value != "None" && value != "Track" && value != "Playlist")
        return fail("Invalid repeat mode.");
      property = "LoopStatus";
      content = g_variant_new_string(value.c_str());
    }
    reply = callSync(
        bus->connection.get(), selected.c_str(), kPath, kProperties, "Set",
        g_variant_new("(ssv)", kPlayer, property.c_str(), content),
        &error);
  } else if (action == "seek") {
    char *end = nullptr;
    const long long position = std::strtoll(value.c_str(), &end, 10);
    if (value.empty() || end == value.c_str() || *end != '\0' || position < 0 ||
        position > current.value("lengthUs", int64_t(0)) || trackId.empty() ||
        trackId != current.value("trackId", std::string{}) ||
        trackId == "/org/mpris/MediaPlayer2/TrackList/NoTrack")
      return fail("The track changed or its position is unavailable.");
    reply = callSync(bus->connection.get(), selected.c_str(), kPath, kPlayer,
                     "SetPosition",
                     g_variant_new("(ox)", trackId.c_str(), position), &error);
  } else {
    return fail("Unknown media action.");
  }

  if (!reply)
    return fail(error.empty() ? "The media player rejected the action."
                              : error);
  return mediaStatus(selected, current.value("bus", std::string{}));
}

} // namespace LunaDash
