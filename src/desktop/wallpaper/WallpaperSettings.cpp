#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QUuid>

#include <cstdint>
#include <limits>
#include <string_view>

namespace LunaDash {
namespace {
constexpr int kWallpaperHistoryLimit = 12;
constexpr int kWallpaperFavoriteLimit = 256;

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

using Json = nlohmann::json;

Json wallpaperSettings() {
  ensureLegacyDesktopSettingsImported();
  return loadWallpaperSettings();
}

QString settingString(const Json &settings, std::string_view key) {
  const auto value = settings.find(key);
  return value != settings.end() && value->is_string()
             ? QString::fromStdString(value->get<std::string>())
             : QString{};
}

QStringList settingStringList(const Json &settings, std::string_view key) {
  QStringList result;
  const auto value = settings.find(key);
  if (value == settings.end() || !value->is_array())
    return result;
  for (const auto &entry : *value)
    if (entry.is_string())
      result.append(QString::fromStdString(entry.get<std::string>()));
  return result;
}

Json toJson(const QStringList &values) {
  auto result = Json::array();
  for (const auto &value : values)
    result.push_back(value.toStdString());
  return result;
}

QString activeWallpaperPath(const Json &settings) {
  if (settings.value("mode", std::string("image")) == "shader")
    return {};
  const QString configured =
      canonicalExistingFile(settingString(settings, "image"));
  return configured.isEmpty() ? bundledWallpaperPath() : configured;
}

QStringList normalizedWallpaperHistory(const Json &settings) {
  const QStringList source = settingStringList(settings, "history");
  QStringList history;
  for (const auto &entry : source) {
    const QString canonical = canonicalExistingFile(entry);
    if (!canonical.isEmpty() && !history.contains(canonical))
      history.append(canonical);
    if (history.size() >= kWallpaperHistoryLimit)
      break;
  }

  return history;
}

void rememberWallpaper(Json &settings, const QString &path) {
  const QString canonical = canonicalExistingFile(path);
  const QString bundled = bundledWallpaperPath();
  if (canonical.isEmpty() || canonical == bundled)
    return;

  QStringList history = normalizedWallpaperHistory(settings);
  history.removeAll(canonical);
  history.prepend(canonical);
  while (history.size() > kWallpaperHistoryLimit)
    history.removeLast();
  settings["history"] = toJson(history);
}

void bumpWallpaperRevision(Json &settings) {
  auto revision = settings.value("revision", std::uint64_t{0});
  if (revision < std::numeric_limits<std::uint64_t>::max())
    ++revision;
  settings["revision"] = revision;
}

QJsonObject wallpaperEntry(const QString &path, const QString &current,
                           bool bundled) {
  const bool video = wallpaperIsVideo(path);
  const QString poster = video ? wallpaperPoster(path) : path;
  const QString playback = video ? wallpaperPlaybackPath(path) : path;
  return QJsonObject{{"path", path},
                     {"url", playback.isEmpty()
                                 ? QString()
                                 : QUrl::fromLocalFile(playback).toString()},
                     {"preview", QFileInfo::exists(poster)
                                     ? QUrl::fromLocalFile(poster).toString()
                                     : QString{}},
                     {"name", QFileInfo(path).completeBaseName()},
                     {"category", bundled ? QStringLiteral("Bundled")
                                          : QFileInfo(path).dir().dirName()},
                     {"type", video ? "video" : "image"},
                     {"bundled", bundled},
                     {"current", path == current}};
}
} // namespace

bool wallpaperIsVideo(const QString &path) {
  return QStringList{"mp4", "webm", "mkv", "mov", "m4v"}.contains(
      QFileInfo(path).suffix().toLower());
}

QString wallpaperImageUrl() {
  const auto settings = wallpaperSettings();
  const QString path = activeWallpaperPath(settings);
  return path.isEmpty() ? QString{} : QUrl::fromLocalFile(path).toString();
}

QJsonArray wallpaperLibrarySnapshot() {
  auto settings = wallpaperSettings();
  const QString current = activeWallpaperPath(settings);
  const QString bundled = bundledWallpaperPath();
  QStringList history = normalizedWallpaperHistory(settings);
  if (history != settingStringList(settings, "history")) {
    std::string error;
    saveWallpaperSettings({{"history", toJson(history)}}, &error);
    settings["history"] = toJson(history);
  }

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
  // Favorites remain reachable after switching libraries or rotating history.
  // Ignore stale paths without ever making the shell open arbitrary file types.
  const QStringList favorites = settingStringList(settings, "favorites");
  int favoriteCount = 0;
  for (const auto &entry : favorites) {
    if (++favoriteCount > kWallpaperFavoriteLimit)
      break;
    const QString path = canonicalExistingFile(entry);
    const auto suffix = QFileInfo(path).suffix().toLower();
    const bool supported =
        wallpaperIsVideo(path) ||
        QStringList{"png", "jpg", "jpeg", "webp", "gif", "bmp"}.contains(
            suffix);
    if (path.isEmpty() || !supported || path == current || path == bundled ||
        history.contains(path))
      continue;
    result.append(wallpaperEntry(path, current, false));
  }
  // Bound discovery and cache it between shell polls. Categories are folders
  // one level below the user-selected library; symlinks are not traversed.
  static QString cachedDirectory;
  static QStringList library;
  static QString cachedRevision;
  static bool initialized = false;
  const QString libraryRevision = settingString(settings, "libraryRefresh");
  const auto directory = QString::fromStdString(
      desktopPreferences().value("wallpaperDirectory", std::string{}));
  if (!initialized || directory != cachedDirectory ||
      cachedRevision != libraryRevision) {
    initialized = true;
    cachedRevision = libraryRevision;
    cachedDirectory = directory;
    library.clear();
    const auto addDirectory = [](const QString &folder) {
      if (!QDir::isAbsolutePath(folder) || !QFileInfo(folder).isDir())
        return;
      QDirIterator entries(folder,
                           QDir::Files | QDir::Readable | QDir::NoSymLinks);
      int inspected = 0;
      while (entries.hasNext() && library.size() < 512 && ++inspected <= 2048) {
        const auto entry = entries.next();
        const auto suffix = QFileInfo(entry).suffix().toLower();
        if (wallpaperIsVideo(entry) ||
            QStringList{"png", "jpg", "jpeg", "webp", "gif", "bmp"}.contains(
                suffix))
          library.append(QFileInfo(entry).canonicalFilePath());
      }
    };
    addDirectory(directory);
    if (QDir::isAbsolutePath(directory) && QFileInfo(directory).isDir()) {
      QDirIterator categories(directory, QDir::Dirs | QDir::NoDotAndDotDot |
                                             QDir::NoSymLinks);
      int count = 0;
      while (categories.hasNext() && library.size() < 512 && ++count <= 64)
        addDirectory(categories.next());
    }
    library.sort(Qt::CaseInsensitive);
  }
  for (const auto &path : library) {
    if (path != bundled && path != current && !history.contains(path) &&
        !favorites.contains(path))
      result.append(wallpaperEntry(path, current, false));
  }
  return result;
}

QJsonObject wallpaperSnapshot() {
  const auto settings = wallpaperSettings();
  const auto path = activeWallpaperPath(settings);
  auto result = wallpaperMediaStatus();
  result["current"] =
      wallpaperEntry(path, path, path == bundledWallpaperPath());
  auto library = wallpaperLibrarySnapshot();
  const QStringList favorites = settingStringList(settings, "favorites");
  for (qsizetype i = 0; i < library.size(); ++i) {
    auto entry = library[i].toObject();
    entry["favorite"] = favorites.contains(entry.value("path").toString());
    library[i] = entry;
  }
  result["library"] = library;
  const QString directory = QString::fromStdString(
      desktopPreferences().value("wallpaperDirectory", std::string{}));
  const QFileInfo info(directory);
  result["directory"] = directory;
  result["resolvedDirectory"] =
      info.exists() ? info.canonicalFilePath() : QDir::cleanPath(directory);
  result["directoryExists"] = info.isDir();
  result["directoryUrl"] = QUrl::fromLocalFile(directory).toString();
  return result;
}

bool refreshWallpaperLibrary(QString *error) {
  ensureLegacyDesktopSettingsImported();
  std::string nativeError;
  if (!saveWallpaperSettings(
          {{"libraryRefresh", QUuid::createUuid().toString().toStdString()}},
          &nativeError)) {
    if (error)
      *error = QString::fromStdString(nativeError);
    return false;
  }
  return true;
}

bool createWallpaperDirectory(QString *error) {
  const QString directory = QString::fromStdString(
      desktopPreferences().value("wallpaperDirectory", std::string{}));
  if (!QDir::isAbsolutePath(directory) || !QDir().mkpath(directory)) {
    if (error)
      *error = "Could not create the wallpaper library directory.";
    return false;
  }
  return refreshWallpaperLibrary(error);
}

bool setWallpaperFavorite(const QString &path, bool favorite, QString *error) {
  const QString canonical = canonicalExistingFile(path);
  if (canonical.isEmpty() ||
      (!wallpaperIsVideo(canonical) && !QImageReader(canonical).canRead())) {
    if (error)
      *error = "The wallpaper is no longer readable.";
    return false;
  }
  auto settings = wallpaperSettings();
  QStringList favorites = settingStringList(settings, "favorites");
  favorites.removeDuplicates();
  favorites.removeAll(canonical);
  if (favorite)
    favorites.prepend(canonical);
  while (favorites.size() > kWallpaperFavoriteLimit)
    favorites.removeLast();
  std::string nativeError;
  if (!saveWallpaperSettings({{"favorites", toJson(favorites)}},
                             &nativeError)) {
    if (error)
      *error = QString::fromStdString(nativeError);
    return false;
  }
  return true;
}

bool selectRandomWallpaper(QString *error) {
  auto candidates = wallpaperLibrarySnapshot();
  for (qsizetype index = candidates.size(); index-- > 0;)
    if (candidates[index].toObject().value("current").toBool())
      candidates.removeAt(index);
  if (candidates.isEmpty()) {
    if (error)
      *error = "Add another image or video to the wallpaper library first.";
    return false;
  }
  const auto index =
      QRandomGenerator::global()->bounded(int(candidates.size()));
  return setWallpaperImage(
      candidates[index].toObject().value("path").toString(), error);
}

quint64 wallpaperRevision() {
  return wallpaperSettings().value("revision", std::uint64_t{0});
}

bool setWallpaperImage(const QString &path, QString *error) {
  const QFileInfo file(path);
  QImageReader reader(file.canonicalFilePath());
  const auto size = reader.size();
  const bool video = wallpaperIsVideo(path);
  const bool validVideo =
      video && file.size() <= qint64(2) * 1024 * 1024 * 1024 &&
      QMimeDatabase()
          .mimeTypeForFile(file, QMimeDatabase::MatchContent)
          .name()
          .startsWith("video/");
  if (!file.isFile() || !file.isReadable() ||
      (video ? !validVideo
             : (file.size() > 64 * 1024 * 1024 || !size.isValid() ||
                static_cast<qint64>(size.width()) * size.height() >
                    32 * 1024 * 1024 ||
                !reader.canRead()))) {
    if (error)
      *error = "Choose an image below 64 MiB / 32 megapixels or a local video "
               "below 2 GiB.";
    return false;
  }

  const QString canonicalPath = file.canonicalFilePath();
  auto settings = wallpaperSettings();
  const QString previous = activeWallpaperPath(settings);
  if (!previous.isEmpty() && previous != canonicalPath)
    rememberWallpaper(settings, previous);

  settings["image"] = canonicalPath.toStdString();
  settings["mode"] = "image";
  rememberWallpaper(settings, canonicalPath);
  bumpWallpaperRevision(settings);
  std::string nativeError;
  if (!saveWallpaperSettings({{"image", settings["image"]},
                              {"mode", settings["mode"]},
                              {"history", settings["history"]},
                              {"revision", settings["revision"]}},
                             &nativeError)) {
    if (error)
      *error = "Could not save the wallpaper selection: " +
               QString::fromStdString(nativeError);
    return false;
  }
  refreshWallpaperPalette(canonicalPath, video);
  return true;
}

void resetWallpaperImage() {
  auto settings = wallpaperSettings();
  const QString previous = activeWallpaperPath(settings);
  const QString bundled = bundledWallpaperPath();
  if (!previous.isEmpty() && previous != bundled)
    rememberWallpaper(settings, previous);
  settings["image"] = "";
  settings["mode"] = "image";
  bumpWallpaperRevision(settings);
  std::string error;
  if (!saveWallpaperSettings({{"image", settings["image"]},
                              {"mode", settings["mode"]},
                              {"history", settings["history"]},
                              {"revision", settings["revision"]}},
                             &error))
    return;
  refreshWallpaperPalette(bundled, false);
}
} // namespace LunaDash
