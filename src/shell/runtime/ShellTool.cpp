#include "shell/runtime/ShellTool.hpp"
#include "desktop/wallpaper/WallpaperActions.hpp"
#include "config/localization/Localization.hpp"
#include "shell/media/MediaPlayer.hpp"
#include "shell/audio/AudioSpectrum.hpp"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

namespace LunaDash {
namespace {
int printNativeJson(const nlohmann::json &object) {
  const auto json = object.dump();
  fwrite(json.data(), 1, json.size(), stdout);
  fputc('\n', stdout);
  return object.contains("error") ? 2 : 0;
}

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
    return printNativeJson(mediaStatus(arguments.value(2).toStdString(),
                                       arguments.value(3).toStdString()));

  if (arguments[1] == "media-action") {
    if (arguments.size() < 3)
      return printJson({{"error", "Expected a media action."}});
    return printNativeJson(mediaAction(
        arguments[2].toStdString(), arguments.value(3).toStdString(),
        arguments.value(4).toStdString(), arguments.value(5).toStdString(),
        arguments.value(6).toStdString()));
  }

  if (arguments[1] == "wallpaper")
    return printJson(wallpaperAction(arguments.mid(2)));

  if (arguments[1] == "language") {
    if (arguments.size() != 3)
      return printJson({{"error", "Expected a locale code."}});
    const QString locale = arguments[2];
    std::string error;
    if (!setSelectedLanguage(locale.toStdString(), &error))
      return printNativeJson({{"error", error}});
    return printJson({{"language", locale}});
  }

  return printJson({{"error", "Unknown shell tool command."}});
}

} // namespace LunaDash
