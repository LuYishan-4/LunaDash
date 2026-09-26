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
QPointer<QProcess> optimization;
QString mediaError;
QString requestedPath;
QString optimizationPath;
QString optimizationTemp;

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

namespace {
QString wallpaperCacheStem(const QString &path) {
  const QFileInfo info(path);
  const QByteArray key =
      (path + QString::number(info.size()) +
       QString::number(info.lastModified().toMSecsSinceEpoch()))
          .toUtf8();
  return QStandardPaths::writableLocation(
             QStandardPaths::GenericCacheLocation) +
         "/lunadash/wallpapers/" +
         QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex();
}
}

QString wallpaperPoster(const QString &path) {
  return wallpaperCacheStem(path) + ".png";
}

QString wallpaperPlaybackPath(const QString &path) {
  if (!wallpaperIsVideo(path))
    return path;
  const QString proxy = wallpaperCacheStem(path) + "-1080p30.mp4";
  if (QFileInfo::exists(proxy))
    return proxy;
  // While the one-time proxy is being built, keep the heavy source off the
  // Qt Quick render loop. If optimization is unavailable or failed, fall back
  // to the original file instead of disabling live wallpaper entirely.
  return optimization && optimizationPath == path ? QString{} : path;
}

QJsonObject wallpaperMediaStatus() {
  return {
      {"extracting", bool(extraction)},
      {"optimizing", bool(optimization)},
      {"optimizedPath", optimizationPath},
      {"error", mediaError},
      {"ffmpegAvailable", !QStandardPaths::findExecutable("ffmpeg").isEmpty()}};
}

void refreshWallpaperPalette(const QString &path, bool video) {
  requestedPath = path;
  mediaError.clear();

  if (optimization && optimizationPath != path) {
    optimization->disconnect();
    QObject::connect(optimization, &QProcess::finished, optimization,
                     &QObject::deleteLater);
    optimization->kill();
    if (!optimizationTemp.isEmpty())
      QFile::remove(optimizationTemp);
    optimization = nullptr;
    optimizationPath.clear();
    optimizationTemp.clear();
  }
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
  const auto ffmpeg = QStandardPaths::findExecutable("ffmpeg");

  // Keep the original file untouched. Live wallpapers play a bounded
  // 1080p/30fps cache proxy so 4K60 sources don't force Qt Quick to decode and
  // composite a full-size frame on every video tick.
  const QString proxy = wallpaperCacheStem(path) + "-1080p30.mp4";
  if (!QFileInfo::exists(proxy) && !optimization && !ffmpeg.isEmpty()) {
    if (QDir().mkpath(QFileInfo(proxy).absolutePath())) {
      auto *process = new QProcess(QCoreApplication::instance());
      optimization = process;
      optimizationPath = path;
      optimizationTemp = proxy + ".part.mp4";
      QFile::remove(optimizationTemp);
      process->setStandardOutputFile(QProcess::nullDevice());
      process->setStandardErrorFile(QProcess::nullDevice());
      QObject::connect(
          process, &QProcess::errorOccurred, process,
          [process](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
              return;
            if (!optimizationTemp.isEmpty())
              QFile::remove(optimizationTemp);
            if (optimization == process) {
              optimization = nullptr;
              optimizationPath.clear();
              optimizationTemp.clear();
            }
            mediaError = "Could not start ffmpeg for the live wallpaper proxy.";
            process->deleteLater();
          });
      QObject::connect(
          process, &QProcess::finished, process,
          [process, path, proxy](int code, QProcess::ExitStatus status) {
            const QString temp = optimizationTemp;
            const bool current = optimization == process &&
                                 optimizationPath == path;
            if (current && code == 0 && status == QProcess::NormalExit &&
                QFileInfo::exists(temp) && QFileInfo(temp).size() > 0) {
              QFile::remove(proxy);
              if (!QFile::rename(temp, proxy))
                mediaError = "Could not publish the optimized wallpaper proxy.";
            } else if (!temp.isEmpty()) {
              QFile::remove(temp);
              if (current && mediaError.isEmpty())
                mediaError = "Could not optimize the live wallpaper.";
            }
            if (optimization == process) {
              optimization = nullptr;
              optimizationPath.clear();
              optimizationTemp.clear();
            }
            process->deleteLater();
          });
      process->start(
          ffmpeg,
          {"-nostdin", "-v", "error", "-y", "-i", path, "-an", "-vf",
           "scale=w='min(iw,1920)':h='min(ih,1080)':force_original_aspect_ratio=decrease:force_divisible_by=2,fps=30",
           "-c:v", "libx264", "-preset", "veryfast", "-crf", "24",
           "-pix_fmt", "yuv420p", "-movflags", "+faststart", "-threads", "1",
           optimizationTemp});
    }
  }

  if (QFileInfo::exists(poster)) {
    extractPalette(poster, path);
    return;
  }
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
