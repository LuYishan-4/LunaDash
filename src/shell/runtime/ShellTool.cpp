#include "shell/runtime/ShellTool.hpp"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QVariantMap>

namespace LunaDash {
namespace {
constexpr auto kMprisPrefix = "org.mpris.MediaPlayer2.";
constexpr auto kMprisPath = "/org/mpris/MediaPlayer2";

QJsonObject mediaSnapshot(const QString &service) {
  auto bus = QDBusConnection::sessionBus();
  QDBusInterface root(service, kMprisPath, "org.mpris.MediaPlayer2", bus);
  QDBusInterface player(service, kMprisPath, "org.mpris.MediaPlayer2.Player",
                        bus);
  if (!root.isValid() || !player.isValid())
    return {};

  const QVariant metadataValue = player.property("Metadata");
  QVariantMap metadata;
  if (metadataValue.canConvert<QVariantMap>())
    metadata = metadataValue.toMap();
  else
    metadata = qdbus_cast<QVariantMap>(metadataValue);

  QStringList artists = metadata.value("xesam:artist").toStringList();
  if (artists.isEmpty() && metadata.value("xesam:artist").isValid())
    artists << metadata.value("xesam:artist").toString();

  const QString status = player.property("PlaybackStatus").toString();
  return {{"available", true},
          {"service", service},
          {"identity", root.property("Identity").toString()},
          {"desktopEntry", root.property("DesktopEntry").toString()},
          {"playbackStatus", status},
          {"playing", status == "Playing"},
          {"title", metadata.value("xesam:title").toString()},
          {"artist", artists.join(", ")},
          {"album", metadata.value("xesam:album").toString()},
          {"artUrl", metadata.value("mpris:artUrl").toString()},
          {"trackUrl", metadata.value("xesam:url").toString()},
          {"lengthUs", metadata.value("mpris:length").toLongLong()},
          {"positionUs", player.property("Position").toLongLong()},
          {"canPlay", player.property("CanPlay").toBool()},
          {"canPause", player.property("CanPause").toBool()},
          {"canGoNext", player.property("CanGoNext").toBool()},
          {"canGoPrevious", player.property("CanGoPrevious").toBool()}};
}

QStringList mediaServices() {
  auto *interface = QDBusConnection::sessionBus().interface();
  if (!interface)
    return {};
  const QDBusReply<QStringList> reply = interface->registeredServiceNames();
  if (!reply.isValid())
    return {};
  QStringList services;
  for (const auto &name : reply.value())
    if (name.startsWith(kMprisPrefix))
      services << name;
  services.sort();
  return services;
}

QJsonObject currentMedia(QString preferred = {}) {
  const auto services = mediaServices();
  if (!preferred.isEmpty() && services.contains(preferred))
    return mediaSnapshot(preferred);

  QJsonObject fallback;
  for (const auto &service : services) {
    const auto snapshot = mediaSnapshot(service);
    if (snapshot.isEmpty())
      continue;
    if (snapshot.value("playing").toBool())
      return snapshot;
    if (fallback.isEmpty())
      fallback = snapshot;
  }
  if (!fallback.isEmpty())
    return fallback;
  return {{"available", false}};
}

int printJson(const QJsonObject &object) {
  const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
  fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
  fputc('\n', stdout);
  return object.contains("error") ? 2 : 0;
}

int mediaAction(const QString &action, const QString &preferred) {
  const auto snapshot = currentMedia(preferred);
  const QString service = snapshot.value("service").toString();
  if (service.isEmpty())
    return printJson({{"error", "No MPRIS media player is available."}});

  static const QHash<QString, QString> methods{{"play-pause", "PlayPause"},
                                               {"play", "Play"},
                                               {"pause", "Pause"},
                                               {"next", "Next"},
                                               {"previous", "Previous"}};
  if (!methods.contains(action))
    return printJson({{"error", "Unknown media action."}});

  QDBusInterface player(service, kMprisPath, "org.mpris.MediaPlayer2.Player",
                        QDBusConnection::sessionBus());
  const auto reply = player.call(methods.value(action));
  if (reply.type() == QDBusMessage::ErrorMessage)
    return printJson({{"error", reply.errorMessage()}});
  return printJson(currentMedia(service));
}
} // namespace

int ShellTool::run(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  app.setApplicationName("LunaDash");
  app.setOrganizationName("LunaDash");

  const QStringList arguments = app.arguments();
  if (arguments.size() < 2)
    return printJson(
        {{"error", "Expected media-status, media-action, or language."}});

  if (arguments[1] == "media-status")
    return printJson(currentMedia(arguments.value(2)));

  if (arguments[1] == "media-action") {
    if (arguments.size() < 3)
      return printJson({{"error", "Expected a media action."}});
    return mediaAction(arguments[2], arguments.value(3));
  }

  if (arguments[1] == "language") {
    if (arguments.size() != 3)
      return printJson({{"error", "Expected a locale code."}});
    const QString locale = arguments[2];
    if (!QRegularExpression("^[a-z]{2,3}(?:_[A-Z]{2})?$")
             .match(locale)
             .hasMatch())
      return printJson({{"error", "Invalid locale code."}});
    QSettings settings;
    settings.setValue("appearance/language", locale);
    settings.sync();
    if (settings.status() != QSettings::NoError)
      return printJson({{"error", "Could not save the language."}});
    return printJson({{"language", locale}});
  }

  return printJson({{"error", "Unknown shell tool command."}});
}

} // namespace LunaDash
