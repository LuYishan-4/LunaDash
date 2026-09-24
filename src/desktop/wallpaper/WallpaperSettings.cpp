#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "config/desktop/DesktopPreferences.hpp"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

namespace LunaDash {
namespace {
constexpr int kWallpaperHistoryLimit = 12;

QString bundledWallpaperPath() {
  auto path = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                     "ludash/wallpapers/florist.png");
  if (path.isEmpty())
    path = QStringLiteral(LUDASH_WALLPAPER_SOURCE_DIR) + "/florist.png";
  return QFileInfo::exists(path) ? QFileInfo(path).canonicalFilePath()
                                 : QString{};
}

QString canonicalExistingFile(const QString &path) {
  const QFileInfo file(path);
  if (!file.isFile() || !file.isReadable())
    return {};
  return file.canonicalFilePath();
}

QString activeWallpaperPath(QSettings &settings) {
  if (settings.value("appearance/wallpaperMode", "image").toString() ==
      "shader")
    return {};
  const QString configured = canonicalExistingFile(
      settings.value("appearance/wallpaperImage").toString());
  return configured.isEmpty() ? bundledWallpaperPath() : configured;
}

QStringList wallpaperHistory(QSettings &settings) {
  QStringList source =
      settings.value("appearance/wallpaperHistory").toStringList();
  if (source.isEmpty())
    source = settings.value("desktop/wallpaperHistory").toStringList();

  QStringList history;
  for (const auto &entry : source) {
    const QString canonical = canonicalExistingFile(entry);
    if (!canonical.isEmpty() && !history.contains(canonical))
      history.append(canonical);
    if (history.size() >= kWallpaperHistoryLimit)
      break;
  }

  settings.setValue("appearance/wallpaperHistory", history);
  settings.remove("desktop/wallpaperHistory");
  return history;
}

void rememberWallpaper(QSettings &settings, const QString &path) {
  const QString canonical = canonicalExistingFile(path);
  const QString bundled = bundledWallpaperPath();
  if (canonical.isEmpty() || canonical == bundled)
    return;

  QStringList history = wallpaperHistory(settings);
  history.removeAll(canonical);
  history.prepend(canonical);
  while (history.size() > kWallpaperHistoryLimit)
    history.removeLast();
  settings.setValue("appearance/wallpaperHistory", history);
}

void bumpWallpaperRevision(QSettings &settings) {
  const quint64 revision =
      settings.value("appearance/wallpaperRevision", 0).toULongLong();
  settings.setValue("appearance/wallpaperRevision", revision + 1);
}

QJsonObject wallpaperEntry(const QString &path, const QString &current,
                           bool bundled) {
  const bool video = wallpaperIsVideo(path);
  const QString poster = video ? wallpaperPoster(path) : path;
  return QJsonObject{{"path", path},
                     {"url", QUrl::fromLocalFile(path).toString()},
                     {"preview", QFileInfo::exists(poster) ? QUrl::fromLocalFile(poster).toString() : QString{}},
                     {"name", QFileInfo(path).completeBaseName()},
                     {"category", bundled ? QStringLiteral("Bundled") : QFileInfo(path).dir().dirName()},
                     {"type", video ? "video" : "image"},
                     {"bundled", bundled},
                     {"current", path == current}};
}
} // namespace

bool wallpaperIsVideo(const QString &path) {
  return QStringList{"mp4", "webm", "mkv", "mov", "m4v"}.contains(QFileInfo(path).suffix().toLower());
}

QString wallpaperImageUrl() {
  QSettings settings;
  const QString path = activeWallpaperPath(settings);
  return path.isEmpty() ? QString{} : QUrl::fromLocalFile(path).toString();
}

QJsonArray wallpaperLibrarySnapshot() {
  QSettings settings;
  const QString current = activeWallpaperPath(settings);
  const QString bundled = bundledWallpaperPath();
  QStringList history = wallpaperHistory(settings);

  QJsonArray result;
  if (!bundled.isEmpty())
    result.append(wallpaperEntry(bundled, current, true));

  if (!current.isEmpty() && current != bundled)
    result.append(wallpaperEntry(current, current, false));

  for (const auto &path : history) {
    if (path == bundled || path == current)
      continue;
    result.append(wallpaperEntry(path, current, false));
  }
  // Bound discovery and cache it between shell polls. Categories are folders
  // one level below the user-selected library; symlinks are not traversed.
  static QString cachedDirectory;
  static QStringList library;
  static QElapsedTimer age;
  const auto directory = desktopPreferences().value("wallpaperDirectory").toString();
  if (directory != cachedDirectory || !age.isValid() || age.elapsed() > 5000) {
    cachedDirectory = directory;
    library.clear();
    const auto addDirectory = [](const QString &folder) {
      QDirIterator entries(folder, QDir::Files | QDir::Readable | QDir::NoSymLinks);
      int inspected = 0;
      while (entries.hasNext() && library.size() < 512 && ++inspected <= 2048) {
        const auto entry = entries.next();
        const auto suffix = QFileInfo(entry).suffix().toLower();
        if (wallpaperIsVideo(entry) || QStringList{"png", "jpg", "jpeg", "webp", "gif", "bmp"}.contains(suffix))
          library.append(QFileInfo(entry).canonicalFilePath());
      }
    };
    addDirectory(directory);
    QDirIterator categories(directory, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    int count = 0;
    while (categories.hasNext() && library.size() < 512 && ++count <= 64)
      addDirectory(categories.next());
    library.sort(Qt::CaseInsensitive);
    age.restart();
  }
  for (const auto &path : library) {
    if (path != bundled && path != current && !history.contains(path))
      result.append(wallpaperEntry(path, current, false));
  }
  settings.sync();
  return result;
}

QJsonObject wallpaperSnapshot() {
  QSettings settings;
  const auto path = activeWallpaperPath(settings);
  auto result = wallpaperMediaStatus();
  result["current"] = wallpaperEntry(path, path, path == bundledWallpaperPath());
  result["library"] = wallpaperLibrarySnapshot();
  result["directory"] = desktopPreferences().value("wallpaperDirectory");
  return result;
}

bool selectRandomWallpaper(QString *error) {
  auto candidates = wallpaperLibrarySnapshot();
  for (qsizetype index = candidates.size(); index-- > 0;)
    if (candidates[index].toObject().value("current").toBool()) candidates.removeAt(index);
  if (candidates.isEmpty()) {
    if (error) *error = "Add another image or video to the wallpaper library first.";
    return false;
  }
  const auto index = QRandomGenerator::global()->bounded(int(candidates.size()));
  return setWallpaperImage(candidates[index].toObject().value("path").toString(), error);
}

quint64 wallpaperRevision() {
  return QSettings().value("appearance/wallpaperRevision", 0).toULongLong();
}

bool setWallpaperImage(const QString &path, QString *error) {
  const QFileInfo file(path);
  QImageReader reader(file.canonicalFilePath());
  const auto size = reader.size();
  const bool video = wallpaperIsVideo(path);
  const bool validVideo = video && file.size() <= qint64(2) * 1024 * 1024 * 1024 &&
      QMimeDatabase().mimeTypeForFile(file, QMimeDatabase::MatchContent).name().startsWith("video/");
  if (!file.isFile() || !file.isReadable() || (video ? !validVideo : (file.size() > 64 * 1024 * 1024 ||
      !size.isValid() ||
      static_cast<qint64>(size.width()) * size.height() > 32 * 1024 * 1024 ||
      !reader.canRead()))) {
    if (error)
      *error = "Choose an image below 64 MiB / 32 megapixels or a local video below 2 GiB.";
    return false;
  }

  const QString canonicalPath = file.canonicalFilePath();
  QSettings settings;
  const QString previous = activeWallpaperPath(settings);
  if (!previous.isEmpty() && previous != canonicalPath)
    rememberWallpaper(settings, previous);

  settings.setValue("appearance/wallpaperImage", canonicalPath);
  settings.setValue("appearance/wallpaperMode", "image");
  rememberWallpaper(settings, canonicalPath);
  bumpWallpaperRevision(settings);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error) *error = "Could not save the wallpaper selection.";
    return false;
  }
  refreshWallpaperPalette(canonicalPath, video);
  return true;
}

void resetWallpaperImage() {
  QSettings settings;
  const QString previous = activeWallpaperPath(settings);
  const QString bundled = bundledWallpaperPath();
  if (!previous.isEmpty() && previous != bundled)
    rememberWallpaper(settings, previous);
  settings.remove("appearance/wallpaperImage");
  settings.setValue("appearance/wallpaperMode", "image");
  bumpWallpaperRevision(settings);
  settings.sync();
  refreshWallpaperPalette(bundled, false);
}
} // namespace LunaDash
