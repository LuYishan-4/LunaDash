#include <gdk-pixbuf/gdk-pixbuf.h>

#include "config/desktop/DesktopPreferences.hpp"
#include "core/filesystem/TemporaryDirectory.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "support/TestHarness.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace LunaDash {
class WallpaperLibraryTests final {
  TemporaryDirectory root;
  std::string library;
  bool contains(const std::string &path, bool favorite = false) {
    const auto snapshot = wallpaperSnapshot();
    for (const auto &entry : snapshot.at("library")) {
      if (entry.value("path", std::string{}) == path &&
          (!favorite || entry.value("favorite", false)))
        return true;
    }
    return false;
  }
  bool image(const std::string &path) {
    auto *pixels = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 16, 16);
    if (!pixels)
      return false;
    gdk_pixbuf_fill(pixels, 0x0000ffff);
    GError *error = nullptr;
    const bool saved =
        gdk_pixbuf_save(pixels, path.c_str(), "png", &error, nullptr);
    g_object_unref(pixels);
    if (error)
      g_error_free(error);
    return saved;
  }
  std::string rootPath() const { return root.path().string(); }

public:
  void initTestCase() {
    VERIFY(root.isValid());
    ::setenv("XDG_CONFIG_HOME", (rootPath() + "/config").c_str(), 1);
    ::setenv("XDG_CACHE_HOME", (rootPath() + "/cache").c_str(), 1);
    library = rootPath() + "/user wallpapers";
    std::string error;
    VERIFY2(updateDesktopPreferences({{"wallpaperDirectory", library}}, &error),
            error);
  }
  void directoryIsCreatedOnlyOnRequest() {
    VERIFY(!wallpaperSnapshot().value("directoryExists", false));
    VERIFY(!std::filesystem::exists(library));
    std::string error;
    VERIFY2(createWallpaperDirectory(&error), error);
    VERIFY(wallpaperSnapshot().value("directoryExists", false));
    COMPARE(wallpaperSnapshot().value("resolvedDirectory", std::string{}),
            library);
  }
  void refreshDiscoversNewFilesBeforeCacheExpiry() {
    wallpaperLibrarySnapshot();
    const std::string path = library + "/rain and spaces.png";
    VERIFY(image(path));
    std::string error;
    VERIFY2(refreshWallpaperLibrary(&error), error);
    VERIFY2(contains(path), wallpaperSnapshot().dump());
    VERIFY2(setWallpaperImage(path, &error), error);
    COMPARE(wallpaperSnapshot().at("current").value("path", std::string{}),
            path);
  }
  void favoritesSurviveChangingLibrariesAndMissingFilesFail() {
    const std::string outside = rootPath() + "/favorite.png";
    VERIFY(image(outside));
    std::string error;
    VERIFY2(setWallpaperFavorite(outside, true, &error), error);
    VERIFY2(contains(outside, true), wallpaperSnapshot().dump());
    std::string preferencesError;
    VERIFY2(updateDesktopPreferences(
                {{"wallpaperDirectory", rootPath() + "/other"}},
                &preferencesError),
            preferencesError);
    VERIFY(contains(outside, true));
    VERIFY(setWallpaperFavorite(outside, false, &error));
    VERIFY(!contains(outside, true));
    VERIFY(!setWallpaperFavorite(rootPath() + "/missing.png", true, &error));
    VERIFY(!error.empty());
  }
};
} // namespace LunaDash

int main(int argc, char **argv) {
  using Fixture = LunaDash::WallpaperLibraryTests;
  return LunaDash::Test::run<Fixture>(
      "WallpaperLibraryTests",
      {{"initTestCase", &Fixture::initTestCase},
       {"directoryIsCreatedOnlyOnRequest",
        &Fixture::directoryIsCreatedOnlyOnRequest},
       {"refreshDiscoversNewFilesBeforeCacheExpiry",
        &Fixture::refreshDiscoversNewFilesBeforeCacheExpiry},
       {"favoritesSurviveChangingLibrariesAndMissingFilesFail",
        &Fixture::favoritesSurviveChangingLibrariesAndMissingFilesFail}},
      argc, argv);
}
