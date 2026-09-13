#include <LuDash/wallpaper/WallpaperSettings.h>
#include <QFileInfo>
#include <QImageReader>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
namespace LuDash {
QString wallpaperImageUrl() {
    QSettings settings;
    if (settings.value("appearance/wallpaperMode", "image").toString() == "shader") return {};
    auto path = settings.value("appearance/wallpaperImage").toString();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "ludash/wallpapers/florist.png");
        if (path.isEmpty()) path = QStringLiteral(LUDASH_WALLPAPER_SOURCE_DIR) + "/florist.png";
    }
    return QFileInfo::exists(path) ? QUrl::fromLocalFile(path).toString() : QString{};
}
bool setWallpaperImage(const QString& path, QString* error) {
    const QFileInfo file(path);
    QImageReader reader(file.canonicalFilePath());
    const auto size = reader.size();
    if (!file.isFile() || !file.isReadable() || file.size() > 64 * 1024 * 1024 ||
        !size.isValid() || static_cast<qint64>(size.width()) * size.height() > 32 * 1024 * 1024 || !reader.canRead()) {
        if (error) *error = "Choose a readable image smaller than 64 MiB and 32 megapixels.";
        return false;
    }
    QSettings settings;
    settings.setValue("appearance/wallpaperImage", file.canonicalFilePath());
    settings.setValue("appearance/wallpaperMode", "image");
    return true;
}
}
