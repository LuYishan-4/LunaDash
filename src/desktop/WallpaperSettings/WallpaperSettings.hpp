#pragma once

#include <QJsonArray>
#include <QString>

namespace LuDash {
QString wallpaperImageUrl();
QJsonArray wallpaperLibrarySnapshot();
quint64 wallpaperRevision();
bool setWallpaperImage(const QString &path, QString *error = nullptr);
void resetWallpaperImage();
}
