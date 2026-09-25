#include "shell/runtime/ShellTool.hpp"
#include "desktop/wallpaper/WallpaperActions.hpp"
#include "shell/media/MediaPlayer.hpp"
#include "shell/audio/AudioSpectrum.hpp"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QVariantMap>

namespace LunaDash {
namespace {
int printJson(const QJsonObject &object) {
  const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
  fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
  fputc('\n', stdout);
  return object.contains("error") ? 2 : 0;
}

} // namespace

int ShellTool::run(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  app.setApplicationName("LunaDash");
  app.setOrganizationName("LunaDash");

  const QStringList arguments = app.arguments();
  if (arguments.size() < 2)
    return printJson(
        {{"error", "Expected media-status, media-action, audio-spectrum, wallpaper, or language."}});

  if (arguments[1] == "audio-spectrum")
    return runAudioSpectrum();

  if (arguments[1] == "media-status")
    return printJson(mediaStatus(arguments.value(2), arguments.value(3)));

  if (arguments[1] == "media-action") {
    if (arguments.size() < 3)
      return printJson({{"error", "Expected a media action."}});
    return printJson(mediaAction(arguments[2], arguments.value(3),
                                 arguments.value(4), arguments.value(5),
                                 arguments.value(6)));
  }

  if (arguments[1] == "wallpaper")
    return printJson(wallpaperAction(arguments.mid(2)));

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
