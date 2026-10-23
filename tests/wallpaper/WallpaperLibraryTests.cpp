#include <gdk-pixbuf/gdk-pixbuf.h>

#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

namespace LunaDash {
class WallpaperLibraryTests final : public QObject {
  Q_OBJECT
  QTemporaryDir root;
  QString library;
  bool contains(const QString &path, bool favorite = false) {
    const auto snapshot = wallpaperSnapshot();
    for (const auto &entry : snapshot.at("library")) {
      if (entry.value("path", std::string{}) == path.toStdString() &&
          (!favorite || entry.value("favorite", false)))
        return true;
    }
    return false;
  }
  bool image(const QString &path) {
    auto *pixels = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 16, 16);
    if (!pixels)
      return false;
    gdk_pixbuf_fill(pixels, 0x0000ffff);
    GError *error = nullptr;
    const bool saved = gdk_pixbuf_save(pixels, path.toUtf8().constData(),
                                       "png", &error, nullptr);
    g_object_unref(pixels);
    if (error)
      g_error_free(error);
    return saved;
  }
private Q_SLOTS:
  void initTestCase() {
    QVERIFY(root.isValid());
    qputenv("XDG_CONFIG_HOME", (root.path() + "/config").toUtf8());
    qputenv("XDG_CACHE_HOME", (root.path() + "/cache").toUtf8());
    QCoreApplication::setOrganizationName("LunaDashTests");
    QCoreApplication::setApplicationName("WallpaperLibrary");
    library = root.path() + "/user wallpapers";
    std::string error;
    QVERIFY2(updateDesktopPreferences(
                 {{"wallpaperDirectory", library.toStdString()}}, &error),
             error.c_str());
  }
  void directoryIsCreatedOnlyOnRequest() {
    QVERIFY(!wallpaperSnapshot().value("directoryExists", false));
    QVERIFY(!QFileInfo::exists(library));
    std::string error;
    QVERIFY2(createWallpaperDirectory(&error), error.c_str());
    QVERIFY(wallpaperSnapshot().value("directoryExists", false));
    QCOMPARE(QString::fromStdString(
                 wallpaperSnapshot().value("resolvedDirectory", std::string{})),
             library);
  }
  void refreshDiscoversNewFilesBeforeCacheExpiry() {
    wallpaperLibrarySnapshot();
    const QString path = library + "/rain and spaces.png";
    QVERIFY(image(path));
    std::string error;
    QVERIFY2(refreshWallpaperLibrary(&error), error.c_str());
    QVERIFY2(contains(path),
             qPrintable(QString::fromStdString(wallpaperSnapshot().dump())));
    QVERIFY2(setWallpaperImage(path.toStdString(), &error), error.c_str());
    QCOMPARE(QString::fromStdString(
                 wallpaperSnapshot().at("current").value("path", std::string{})),
             path);
  }
  void favoritesSurviveChangingLibrariesAndMissingFilesFail() {
    const QString outside = root.path() + "/favorite.png";
    QVERIFY(image(outside));
    std::string error;
    QVERIFY2(setWallpaperFavorite(outside.toStdString(), true, &error),
             error.c_str());
    QVERIFY2(contains(outside, true),
             qPrintable(QString::fromStdString(wallpaperSnapshot().dump())));
    std::string preferencesError;
    QVERIFY2(updateDesktopPreferences(
                 {{"wallpaperDirectory", (root.path() + "/other").toStdString()}},
                 &preferencesError),
             preferencesError.c_str());
    QVERIFY(contains(outside, true));
    QVERIFY(setWallpaperFavorite(outside.toStdString(), false, &error));
    QVERIFY(!contains(outside, true));
    QVERIFY(!setWallpaperFavorite((root.path() + "/missing.png").toStdString(),
                                  true, &error));
    QVERIFY(!error.empty());
  }
};
} // namespace LunaDash
QTEST_GUILESS_MAIN(LunaDash::WallpaperLibraryTests)
#include "WallpaperLibraryTests.moc"
