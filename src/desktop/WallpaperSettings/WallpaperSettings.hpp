#pragma once
#include <QString>
namespace LuDash {
QString wallpaperImageUrl();
bool setWallpaperImage(const QString& path, QString* error = nullptr);
}
