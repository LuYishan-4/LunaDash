#pragma once
#include <QJsonObject>
#include <QString>

namespace LunaDash {
void refreshWallpaperPalette(const QString &path, bool video);
QString wallpaperPoster(const QString &path);
QString wallpaperPlaybackPath(const QString &path);
QJsonObject wallpaperMediaStatus();
} // namespace LunaDash
