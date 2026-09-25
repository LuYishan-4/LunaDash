#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

namespace LunaDash {
class WallpaperLibraryTests final : public QObject {
  Q_OBJECT
  QTemporaryDir root;
  QString library;
  bool contains(const QString &path, bool favorite = false) {
    for (const auto &value : wallpaperSnapshot().value("library").toArray()) {
      const auto entry = value.toObject();
      if (entry.value("path").toString() == path &&
          (!favorite || entry.value("favorite").toBool())) return true;
    }
    return false;
  }
  bool image(const QString &path) {
    QImage pixels(16, 16, QImage::Format_ARGB32);
    pixels.fill(Qt::blue);
    return pixels.save(path, "PNG");
  }
private Q_SLOTS:
  void initTestCase() {
    QVERIFY(root.isValid());
    qputenv("XDG_CONFIG_HOME", (root.path() + "/config").toUtf8());
    qputenv("XDG_CACHE_HOME", (root.path() + "/cache").toUtf8());
    QCoreApplication::setOrganizationName("LunaDashTests");
    QCoreApplication::setApplicationName("WallpaperLibrary");
    library = root.path() + "/user wallpapers";
    QString error;
    QVERIFY2(updateDesktopPreferences({{"wallpaperDirectory", library}}, &error), qPrintable(error));
  }
  void directoryIsCreatedOnlyOnRequest() {
    QVERIFY(!wallpaperSnapshot().value("directoryExists").toBool());
    QVERIFY(!QFileInfo::exists(library));
    QString error;
    QVERIFY2(createWallpaperDirectory(&error), qPrintable(error));
    QVERIFY(wallpaperSnapshot().value("directoryExists").toBool());
    QCOMPARE(wallpaperSnapshot().value("resolvedDirectory").toString(), library);
  }
  void refreshDiscoversNewFilesBeforeCacheExpiry() {
    wallpaperLibrarySnapshot();
    const QString path = library + "/rain and spaces.png";
    QVERIFY(image(path));
    refreshWallpaperLibrary();
    QVERIFY(contains(path));
    QString error;
    QVERIFY2(setWallpaperImage(path, &error), qPrintable(error));
    QCOMPARE(wallpaperSnapshot().value("current").toObject().value("path").toString(), path);
  }
  void favoritesSurviveChangingLibrariesAndMissingFilesFail() {
    const QString outside = root.path() + "/favorite.png";
    QVERIFY(image(outside));
    QString error;
    QVERIFY2(setWallpaperFavorite(outside, true, &error), qPrintable(error));
    QVERIFY(contains(outside, true));
    QVERIFY2(updateDesktopPreferences({{"wallpaperDirectory", root.path() + "/other"}}, &error), qPrintable(error));
    QVERIFY(contains(outside, true));
    QVERIFY(setWallpaperFavorite(outside, false, &error));
    QVERIFY(!contains(outside, true));
    QVERIFY(!setWallpaperFavorite(root.path() + "/missing.png", true, &error));
    QVERIFY(!error.isEmpty());
  }
};
} // namespace LunaDash
QTEST_GUILESS_MAIN(LunaDash::WallpaperLibraryTests)
#include "WallpaperLibraryTests.moc"
