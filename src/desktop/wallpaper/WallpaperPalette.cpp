#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"
#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <array>

namespace LunaDash {
namespace {
QPointer<QProcess> extraction;
QString mediaError;
QString requestedPath;

void extractPalette(const QString &imagePath, const QString &original) {
  if (!desktopPreferences().value("wallpaperColors").toBool() ||
      QUrl(wallpaperImageUrl()).toLocalFile() != original)
    return;
  QImageReader reader(imagePath);
  reader.setAutoTransform(true);
  reader.setScaledSize(reader.size().scaled(96, 96, Qt::KeepAspectRatio));
  const QImage image = reader.read();
  if (image.isNull())
    return;
  std::array<double, 24> weights{};
  std::array<double, 24> saturation{};
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QColor color = image.pixelColor(x, y);
      if (color.alphaF() < 0.5 || color.lightnessF() < 0.08 ||
          color.lightnessF() > 0.93 || color.hslHueF() < 0)
        continue;
      const int index = std::clamp(int(color.hslHueF() * 24), 0, 23);
      const double weight = 0.05 + color.hslSaturationF();
      weights[index] += weight;
      saturation[index] += color.hslSaturationF() * weight;
    }
  }
  const auto largest = std::max_element(weights.begin(), weights.end());
  if (*largest <= 0)
    return;
  const int index = int(largest - weights.begin());
  const double hue = (index + 0.5) / 24.0;
  const double sat = std::clamp(saturation[index] / *largest, 0.25, 0.65);
  QString error;
  updateDesktopPreferences(
      {{"accent", QColor::fromHslF(hue, sat, 0.78).name()},
       {"secondaryAccent", QColor::fromHslF(hue, 0.22, 0.65).name()}},
      &error);
  if (!error.isEmpty())
    mediaError = error;
}
} // namespace

QString wallpaperPoster(const QString &path) {
  const QFileInfo info(path);
  const QByteArray key =
      (path + QString::number(info.size()) +
       QString::number(info.lastModified().toMSecsSinceEpoch()))
          .toUtf8();
  return QStandardPaths::writableLocation(
             QStandardPaths::GenericCacheLocation) +
         "/lunadash/wallpapers/" +
         QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex() +
         ".png";
}

QJsonObject wallpaperMediaStatus() {
  return {
      {"extracting", bool(extraction)},
      {"error", mediaError},
      {"ffmpegAvailable", !QStandardPaths::findExecutable("ffmpeg").isEmpty()}};
}

void refreshWallpaperPalette(const QString &path, bool video) {
  requestedPath = path;
  mediaError.clear();
  if (extraction) {
    extraction->disconnect();
    QObject::connect(extraction, &QProcess::finished, extraction,
                     &QObject::deleteLater);
    extraction->kill();
    extraction = nullptr;
  }
  if (!video) {
    extractPalette(path, path);
    return;
  }
  const auto poster = wallpaperPoster(path);
  if (QFileInfo::exists(poster)) {
    extractPalette(poster, path);
    return;
  }
  const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg");
  if (ffmpeg.isEmpty()) {
    mediaError =
        "Install ffmpeg to generate live wallpaper previews and colors.";
    return;
  }
  if (!QDir().mkpath(QFileInfo(poster).absolutePath())) {
    mediaError = "Could not create the wallpaper preview cache.";
    return;
  }
  auto *process = new QProcess(QCoreApplication::instance());
  extraction = process;
  auto *timeout = new QTimer(process);
  timeout->setSingleShot(true);
  QObject::connect(timeout, &QTimer::timeout, process,
                   [process] { process->kill(); });
  QObject::connect(process, &QProcess::errorOccurred, process,
                   [process](QProcess::ProcessError error) {
                     if (error == QProcess::FailedToStart) {
                       mediaError =
                           "Could not start ffmpeg for the wallpaper preview.";
                       if (extraction == process)
                         extraction = nullptr;
                       process->deleteLater();
                     }
                   });
  QObject::connect(
      process, &QProcess::finished, process,
      [process, timeout, path, poster](int code, QProcess::ExitStatus status) {
        timeout->stop();
        if (requestedPath == path) {
          if (code == 0 && status == QProcess::NormalExit &&
              QFileInfo::exists(poster))
            extractPalette(poster, path);
          else {
            QFile::remove(poster);
            mediaError = "Could not decode a preview frame from this video.";
          }
        }
        if (extraction == process)
          extraction = nullptr;
        process->deleteLater();
      });
  process->setStandardOutputFile(QProcess::nullDevice());
  process->setStandardErrorFile(QProcess::nullDevice());
  process->start(ffmpeg,
                 {"-nostdin", "-v", "error", "-y", "-protocol_whitelist",
                  "file,pipe", "-i", path, "-frames:v", "1", "-vf",
                  "scale=640:-2", "-threads", "1", poster});
  timeout->start(12000);
}
} // namespace LunaDash
