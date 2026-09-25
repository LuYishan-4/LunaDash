#include "shell/media/MediaPlayer.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace LunaDash {
namespace {
constexpr auto kPrefix = "org.mpris.MediaPlayer2.";
constexpr auto kPath = "/org/mpris/MediaPlayer2";
constexpr auto kRoot = "org.mpris.MediaPlayer2";
constexpr auto kPlayer = "org.mpris.MediaPlayer2.Player";
constexpr auto kProperties = "org.freedesktop.DBus.Properties";
constexpr int kTimeout = 700;

struct Bus {
  QString name;
  QDBusConnection connection;
};

QDBusMessage request(const QString &service, const QString &interface,
                     const QString &method, const QVariantList &args = {}) {
  auto message =
      QDBusMessage::createMethodCall(service, kPath, interface, method);
  message.setArguments(args);
  return message;
}

QString busId(const QDBusConnection &bus) {
  const auto message = QDBusMessage::createMethodCall(
      "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
      "GetId");
  const QDBusReply<QString> reply = bus.call(message, QDBus::Block, kTimeout);
  return reply.isValid() ? reply.value() : QString();
}

std::vector<Bus> mediaBuses() {
  std::vector<Bus> result;
  const auto session = QDBusConnection::sessionBus();
  QString sessionId;
  if (session.isConnected()) {
    result.push_back({"session", session});
    sessionId = busId(session);
  }
  // A display-manager session may have its own bus while user services and
  // sandboxed applications advertise players on the login user's bus.
  const QString socket =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
      "/bus";
  if (QFileInfo::exists(socket)) {
    const auto user = QDBusConnection::connectToBus("unix:path=" + socket,
                                                    "lunadash-media-user");
    if (user.isConnected() && (sessionId.isEmpty() || busId(user) != sessionId))
      result.push_back({"user", user});
  }
  return result;
}

QVariantMap mapValue(const QVariant &value) {
  return value.canConvert<QVariantMap>() ? value.toMap()
                                         : qdbus_cast<QVariantMap>(value);
}

QString lyrics(const QVariantMap &metadata) {
  for (const auto *key : {"xesam:lyrics", "lyrics", "xesam:asText"}) {
    const QString value = metadata.value(key).toString();
    if (!value.isEmpty())
      return value.left(131072);
  }
  const QUrl url(metadata.value("xesam:url").toString());
  if (!url.isLocalFile())
    return {};
  const QFileInfo track(url.toLocalFile());
  QFile file(track.path() + "/" + track.completeBaseName() + ".lrc");
  if (!QFileInfo(file).isFile() || file.size() > 262144 ||
      !file.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(file.read(262144));
}

QJsonObject snapshot(const QString &service, const QString &bus,
                     const QVariantMap &root, const QVariantMap &player) {
  if (!player.contains("PlaybackStatus"))
    return {};
  const auto metadata = mapValue(player.value("Metadata"));
  auto artists = metadata.value("xesam:artist").toStringList();
  if (artists.isEmpty() && metadata.value("xesam:artist").isValid())
    artists << metadata.value("xesam:artist").toString();
  const QString status = player.value("PlaybackStatus").toString();
  const bool control = player.value("CanControl", true).toBool();
  const auto trackId =
      qdbus_cast<QDBusObjectPath>(metadata.value("mpris:trackid")).path();
  QJsonObject result{
      {"available", true},
      {"service", service},
      {"bus", bus},
      {"identity",
       root.value("Identity", service.mid(QString(kPrefix).size())).toString()},
      {"desktopEntry", root.value("DesktopEntry").toString()},
      {"playbackStatus", status},
      {"playing", status == "Playing"},
      {"title", metadata.value("xesam:title").toString()},
      {"artist", artists.join(", ")},
      {"album", metadata.value("xesam:album").toString()},
      {"artUrl", metadata.value("mpris:artUrl").toString()},
      {"trackUrl", metadata.value("xesam:url").toString()},
      {"trackId", trackId},
      {"lengthUs", metadata.value("mpris:length").toLongLong()},
      {"positionUs", player.value("Position").toLongLong()},
      {"positionSupported", player.contains("Position")},
      {"rate", player.value("Rate", 1.0).toDouble()},
      {"volume", player.value("Volume", 0.0).toDouble()},
      {"volumeSupported", control && player.contains("Volume")},
      {"shuffle", player.value("Shuffle").toBool()},
      {"shuffleSupported", control && player.contains("Shuffle")},
      {"loopStatus", player.value("LoopStatus", "None").toString()},
      {"loopSupported", control && player.contains("LoopStatus")},
      {"canRaise", root.value("CanRaise").toBool()},
      {"lyrics", lyrics(metadata)}};
  for (const auto *capability :
       {"Play", "Pause", "GoNext", "GoPrevious", "Seek"})
    result.insert(QString("can") + capability,
                  control &&
                      player.value(QString("Can") + capability).toBool());
  return result;
}

struct Player {
  QString service;
  QString bus;
  QVariantMap root;
  QVariantMap properties;
};

QList<QJsonObject> collect(const std::vector<Bus> &buses) {
  QEventLoop loop;
  std::vector<std::shared_ptr<Player>> pending;
  int calls = 0;
  for (const auto &bus : buses) {
    auto message = QDBusMessage::createMethodCall(
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "ListNames");
    const QDBusReply<QStringList> names =
        bus.connection.call(message, QDBus::Block, kTimeout);
    if (!names.isValid())
      continue;
    QStringList services = names.value();
    services.sort();
    for (const auto &name : services) {
      if (!name.startsWith(kPrefix))
        continue;
      auto player = std::make_shared<Player>(Player{name, bus.name, {}, {}});
      pending.push_back(player);
      for (const bool root : {true, false}) {
        ++calls;
        auto *watcher = new QDBusPendingCallWatcher(
            bus.connection.asyncCall(request(name, kProperties, "GetAll",
                                             {QString(root ? kRoot : kPlayer)}),
                                     kTimeout),
            &loop);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &loop,
                         [&, player, root](QDBusPendingCallWatcher *finished) {
                           const QDBusMessage reply = finished->reply();
                           if (reply.type() != QDBusMessage::ErrorMessage &&
                               !reply.arguments().isEmpty())
                             (root ? player->root : player->properties) =
                                 mapValue(reply.arguments().first());
                           if (--calls == 0)
                             loop.quit();
                         });
      }
    }
  }
  if (calls) {
    QTimer::singleShot(kTimeout + 100, &loop, &QEventLoop::quit);
    loop.exec();
  }
  QList<QJsonObject> result;
  for (const auto &player : pending) {
    auto data = snapshot(player->service, player->bus, player->root,
                         player->properties);
    if (!data.isEmpty())
      result.append(data);
  }
  return result;
}

QJsonObject select(const QList<QJsonObject> &players, const QString &service,
                   const QString &bus) {
  QJsonArray list;
  QJsonObject selected;
  for (const auto &player : players) {
    list.append(QJsonObject{{"service", player["service"]},
                            {"bus", player["bus"]},
                            {"identity", player["identity"]},
                            {"playing", player["playing"]}});
    if (player["service"].toString() == service &&
        (bus.isEmpty() || player["bus"].toString() == bus))
      selected = player;
  }
  if (selected.isEmpty())
    for (const auto &player : players)
      if (selected.isEmpty() ||
          (!selected["playing"].toBool() && player["playing"].toBool()))
        selected = player;
  if (selected.isEmpty())
    selected = {{"available", false}};
  selected["players"] = list;
  return selected;
}
} // namespace

QJsonObject mediaStatus(const QString &service, const QString &bus) {
  return select(collect(mediaBuses()), service, bus);
}

QJsonObject mediaAction(const QString &action, const QString &service,
                        const QString &busName, const QString &value,
                        const QString &trackId) {
  const auto buses = mediaBuses();
  const auto players = collect(buses);
  auto current = select(players, service, busName);
  const auto fail = [&](const QString &error) {
    auto result = current;
    result["error"] = error;
    return result;
  };
  if (!current["available"].toBool() ||
      (!service.isEmpty() &&
       (current["service"].toString() != service ||
        (!busName.isEmpty() && current["bus"].toString() != busName))))
    return fail("The selected media player is no longer available.");
  const auto bus =
      std::find_if(buses.begin(), buses.end(), [&](const Bus &entry) {
        return entry.name == current["bus"].toString();
      });
  if (bus == buses.end())
    return fail("The media connection is unavailable.");
  const QString selected = current["service"].toString();
  static const QHash<QString, QString> methods{
      {"play-pause", "PlayPause"}, {"play", "Play"},
      {"pause", "Pause"},          {"next", "Next"},
      {"previous", "Previous"},    {"raise", "Raise"}};
  QDBusMessage message;
  if (methods.contains(action)) {
    message =
        request(selected, action == "raise" ? kRoot : kPlayer, methods[action]);
  } else if (action == "volume" || action == "shuffle" || action == "loop") {
    QString property;
    QVariant content;
    if (action == "volume") {
      bool valid = false;
      const double volume = value.toDouble(&valid);
      if (!valid || !std::isfinite(volume) || volume < 0 || volume > 1)
        return fail("Invalid media volume.");
      property = "Volume";
      content = volume;
    } else if (action == "shuffle") {
      if (value != "true" && value != "false")
        return fail("Invalid shuffle state.");
      property = "Shuffle";
      content = value == "true";
    } else {
      if (!QStringList{"None", "Track", "Playlist"}.contains(value))
        return fail("Invalid repeat mode.");
      property = "LoopStatus";
      content = value;
    }
    message = request(selected, kProperties, "Set",
                      {QString(kPlayer), property,
                       QVariant::fromValue(QDBusVariant(content))});
  } else if (action == "seek") {
    bool valid = false;
    const qint64 position = value.toLongLong(&valid);
    if (!valid || position < 0 || position > current["lengthUs"].toInteger() ||
        trackId.isEmpty() || trackId != current["trackId"].toString() ||
        trackId == "/org/mpris/MediaPlayer2/TrackList/NoTrack")
      return fail("The track changed or its position is unavailable.");
    message =
        request(selected, kPlayer, "SetPosition",
                {QVariant::fromValue(QDBusObjectPath(trackId)), position});
  } else
    return fail("Unknown media action.");
  const auto reply = bus->connection.call(message, QDBus::Block, kTimeout);
  if (reply.type() == QDBusMessage::ErrorMessage)
    return fail(reply.errorMessage());
  return mediaStatus(selected, current["bus"].toString());
}
} // namespace LunaDash
