#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace LunaDash {
QString wallpaperImageUrl();
QJsonArray wallpaperLibrarySnapshot();
QJsonObject wallpaperSnapshot();
bool wallpaperIsVideo(const QString &path);
bool refreshWallpaperLibrary(QString *error = nullptr);
bool createWallpaperDirectory(QString *error);
bool setWallpaperFavorite(const QString &path, bool favorite, QString *error);
bool selectRandomWallpaper(QString *error);
quint64 wallpaperRevision();
bool setWallpaperImage(const QString &path, QString *error = nullptr);
void resetWallpaperImage();
} // namespace LunaDash
