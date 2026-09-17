#include "desktop/WallpaperSettings/WallpaperSettings.hpp"

#include <QFileInfo>
#include <QImageReader>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

namespace LuDash {
namespace {
constexpr int kWallpaperHistoryLimit = 12;

QString bundledWallpaperPath() {
    auto path = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                       "ludash/wallpapers/florist.png");
    if (path.isEmpty())
        path = QStringLiteral(LUDASH_WALLPAPER_SOURCE_DIR) + "/florist.png";
    return QFileInfo::exists(path) ? QFileInfo(path).canonicalFilePath() : QString{};
}

QString canonicalExistingFile(const QString &path) {
    const QFileInfo file(path);
    if (!file.isFile() || !file.isReadable())
        return {};
    return file.canonicalFilePath();
}

QString activeWallpaperPath(QSettings &settings) {
    if (settings.value("appearance/wallpaperMode", "image").toString() == "shader")
        return {};
    const QString configured = canonicalExistingFile(
        settings.value("appearance/wallpaperImage").toString());
    return configured.isEmpty() ? bundledWallpaperPath() : configured;
}

QStringList wallpaperHistory(QSettings &settings) {
    QStringList source = settings.value("appearance/wallpaperHistory").toStringList();
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
    const quint64 revision = settings.value("appearance/wallpaperRevision", 0).toULongLong();
    settings.setValue("appearance/wallpaperRevision", revision + 1);
}

QJsonObject wallpaperEntry(const QString &path, const QString &current,
                           bool bundled) {
    return QJsonObject{{"path", path},
                       {"url", QUrl::fromLocalFile(path).toString()},
                       {"bundled", bundled},
                       {"current", path == current}};
}
} // namespace

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
    settings.sync();
    return result;
}

quint64 wallpaperRevision() {
    return QSettings().value("appearance/wallpaperRevision", 0).toULongLong();
}

bool setWallpaperImage(const QString &path, QString *error) {
    const QFileInfo file(path);
    QImageReader reader(file.canonicalFilePath());
    const auto size = reader.size();
    if (!file.isFile() || !file.isReadable() || file.size() > 64 * 1024 * 1024 ||
        !size.isValid() || static_cast<qint64>(size.width()) * size.height() > 32 * 1024 * 1024 ||
        !reader.canRead()) {
        if (error)
            *error = "Choose a readable image smaller than 64 MiB and 32 megapixels.";
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
}
} // namespace LuDash
