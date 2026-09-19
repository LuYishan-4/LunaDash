#pragma once

#include <QJsonArray>
#include <QString>

namespace LunaDash {
QString wallpaperImageUrl();
QJsonArray wallpaperLibrarySnapshot();
quint64 wallpaperRevision();
bool setWallpaperImage(const QString &path, QString *error = nullptr);
void resetWallpaperImage();
} // namespace LunaDash
