#include "desktop/wallpaper/WallpaperSettings.hpp"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>

#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <unistd.h>

namespace LunaDash {
namespace {
constexpr std::size_t kWallpaperHistoryLimit = 12;
constexpr std::size_t kWallpaperFavoriteLimit = 256;
using Json = nlohmann::json;
using PathList = std::vector<std::filesystem::path>;

std::filesystem::path bundledWallpaperPath() {
  std::vector<std::filesystem::path> roots;
  roots.emplace_back(g_get_user_data_dir());
  for (const auto *const *directory = g_get_system_data_dirs(); *directory;
       ++directory)
    roots.emplace_back(*directory);
  for (const auto &root : roots) {
    const auto candidate = root / "ludash/wallpapers/florist.png";
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error))
      return std::filesystem::canonical(candidate, error);
  }
  const auto fallback =
      std::filesystem::path(LUDASH_WALLPAPER_SOURCE_DIR) / "florist.png";
  std::error_code error;
  return std::filesystem::is_regular_file(fallback, error) ? fallback
                                                            : std::filesystem::path{};
}

std::filesystem::path canonicalExistingFile(const std::filesystem::path &path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error) ||
      ::access(path.c_str(), R_OK) != 0)
    return {};
  auto canonical = std::filesystem::canonical(path, error);
  return error ? std::filesystem::path{} : canonical;
}

Json wallpaperSettings() {
  ensureLegacyDesktopSettingsImported();
  return loadWallpaperSettings();
}

std::string settingString(const Json &settings, std::string_view key) {
  const auto value = settings.find(key);
  return value != settings.end() && value->is_string()
             ? value->get<std::string>()
             : std::string{};
}

PathList settingPathList(const Json &settings, std::string_view key) {
  PathList result;
  const auto value = settings.find(key);
  if (value == settings.end() || !value->is_array())
    return result;
  for (const auto &entry : *value)
    if (entry.is_string())
      result.emplace_back(entry.get<std::string>());
  return result;
}

Json toJson(const PathList &values) {
  auto result = Json::array();
  for (const auto &value : values)
    result.push_back(value.string());
  return result;
}

std::filesystem::path activeWallpaperPath(const Json &settings) {
  if (settings.value("mode", std::string("image")) == "shader")
    return {};
  const auto configured =
      canonicalExistingFile(settingString(settings, "image"));
  return configured.empty() ? bundledWallpaperPath() : configured;
}

std::string fileUrl(const std::filesystem::path &path) {
  if (path.empty())
    return {};
  GError *error = nullptr;
  gchar *uri = g_filename_to_uri(path.c_str(), nullptr, &error);
  if (!uri) {
    if (error)
      g_error_free(error);
    return {};
  }
  std::string result(uri);
  g_free(uri);
  return result;
}

bool supportedImage(const std::filesystem::path &path) {
  static const std::vector<std::string> extensions{
      ".png", ".jpg", ".jpeg", ".webp", ".gif", ".bmp"};
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  return std::find(extensions.begin(), extensions.end(), extension) !=
         extensions.end();
}

bool readableImage(const std::filesystem::path &path) {
  int width = 0;
  int height = 0;
  return gdk_pixbuf_get_file_info(path.c_str(), &width, &height) != nullptr &&
         width > 0 && height > 0;
}

bool isVideoContent(const std::filesystem::path &path) {
  std::array<char, 8192> header{};
  std::ifstream input(path, std::ios::binary);
  input.read(header.data(), header.size());
  const auto length = static_cast<gsize>(input.gcount());
  gboolean uncertain = FALSE;
  gchar *contentType =
      g_content_type_guess(path.c_str(), reinterpret_cast<const guchar *>(
                                                 header.data()),
                           length, &uncertain);
  if (!contentType)
    return false;
  gchar *mimeType = g_content_type_get_mime_type(contentType);
  const bool video = mimeType && std::string_view(mimeType).starts_with("video/");
  g_free(mimeType);
  g_free(contentType);
  return video;
}

bool supportedWallpaper(const std::filesystem::path &path) {
  return wallpaperIsVideo(path) || supportedImage(path);
}

void rememberWallpaper(Json &settings, const std::filesystem::path &path) {
  const auto canonical = canonicalExistingFile(path);
  const auto bundled = bundledWallpaperPath();
  if (canonical.empty() || canonical == bundled)
    return;

  PathList history;
  for (const auto &entry : settingPathList(settings, "history")) {
    const auto normalized = canonicalExistingFile(entry);
    if (!normalized.empty() && normalized != canonical &&
        std::find(history.begin(), history.end(), normalized) == history.end())
      history.push_back(normalized);
    if (history.size() >= kWallpaperHistoryLimit)
      break;
  }
  history.insert(history.begin(), canonical);
  while (history.size() > kWallpaperHistoryLimit)
    history.pop_back();
  settings["history"] = toJson(history);
}

void bumpWallpaperRevision(Json &settings) {
  auto revision = settings.value("revision", std::uint64_t{0});
  if (revision < std::numeric_limits<std::uint64_t>::max())
    ++revision;
  settings["revision"] = revision;
}

Json wallpaperEntry(const std::filesystem::path &path,
                    const std::filesystem::path &current, bool bundled) {
  const bool video = wallpaperIsVideo(path);
  const auto poster = video ? wallpaperPoster(path) : path;
  const auto playback = video ? wallpaperPlaybackPath(path) : path;
  std::error_code error;
  return {{"path", path.string()},
          {"url", fileUrl(playback)},
          {"preview", std::filesystem::is_regular_file(poster, error)
                          ? fileUrl(poster)
                          : std::string{}},
          {"name", path.stem().string()},
          {"category", bundled ? "Bundled" : path.parent_path().filename().string()},
          {"type", video ? "video" : "image"},
          {"bundled", bundled},
          {"current", path == current}};
}

std::string pathSortKey(const std::filesystem::path &path) {
  auto key = path.string();
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });
  return key;
}
} // namespace

bool wallpaperIsVideo(const std::filesystem::path &path) {
  static const std::vector<std::string> extensions{
      ".mp4", ".webm", ".mkv", ".mov", ".m4v"};
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  return std::find(extensions.begin(), extensions.end(), extension) !=
         extensions.end();
}

std::filesystem::path wallpaperImagePath() {
  return activeWallpaperPath(wallpaperSettings());
}

Json wallpaperLibrarySnapshot() {
  auto settings = wallpaperSettings();
  const auto current = activeWallpaperPath(settings);
  const auto bundled = bundledWallpaperPath();
  PathList history;
  for (const auto &entry : settingPathList(settings, "history")) {
    const auto normalized = canonicalExistingFile(entry);
    if (!normalized.empty() &&
        std::find(history.begin(), history.end(), normalized) == history.end())
      history.push_back(normalized);
    if (history.size() >= kWallpaperHistoryLimit)
      break;
  }
  if (history != settingPathList(settings, "history")) {
    std::string error;
    if (saveWallpaperSettings({{"history", toJson(history)}}, &error))
      settings["history"] = toJson(history);
  }

  Json result = Json::array();
  if (!bundled.empty())
    result.push_back(wallpaperEntry(bundled, current, true));
  if (!current.empty() && current != bundled)
    result.push_back(wallpaperEntry(current, current, false));
  for (const auto &path : history) {
    if (path != bundled && path != current)
      result.push_back(wallpaperEntry(path, current, false));
  }

  const auto favorites = settingPathList(settings, "favorites");
  for (std::size_t index = 0; index < favorites.size() &&
                               index < kWallpaperFavoriteLimit;
       ++index) {
    const auto path = canonicalExistingFile(favorites[index]);
    if (path.empty() || !supportedWallpaper(path) || path == current ||
        path == bundled ||
        std::find(history.begin(), history.end(), path) != history.end())
      continue;
    result.push_back(wallpaperEntry(path, current, false));
  }

  static std::filesystem::path cachedDirectory;
  static PathList library;
  static std::string cachedRevision;
  static bool initialized = false;
  const auto directory = std::filesystem::path(
      desktopPreferences().value("wallpaperDirectory", std::string{}));
  const auto libraryRevision = settingString(settings, "libraryRefresh");
  if (!initialized || directory != cachedDirectory ||
      cachedRevision != libraryRevision) {
    initialized = true;
    cachedRevision = libraryRevision;
    cachedDirectory = directory;
    library.clear();
    const auto addDirectory = [](const std::filesystem::path &folder) {
      std::error_code error;
      if (!folder.is_absolute() ||
          !std::filesystem::is_directory(folder, error))
        return;
      std::filesystem::directory_iterator entries(
          folder, std::filesystem::directory_options::skip_permission_denied,
          error);
      std::size_t inspected = 0;
      for (const auto &entry : entries) {
        if (library.size() >= 512 || ++inspected > 2048)
          break;
        const auto status = entry.symlink_status(error);
        if (error || std::filesystem::is_symlink(status) ||
            !std::filesystem::is_regular_file(status) ||
            ::access(entry.path().c_str(), R_OK) != 0 ||
            !supportedWallpaper(entry.path()))
          continue;
        const auto canonical = canonicalExistingFile(entry.path());
        if (!canonical.empty())
          library.push_back(canonical);
      }
    };

    addDirectory(directory);
    std::error_code error;
    if (directory.is_absolute() &&
        std::filesystem::is_directory(directory, error)) {
      std::filesystem::directory_iterator categories(
          directory, std::filesystem::directory_options::skip_permission_denied,
          error);
      std::size_t count = 0;
      for (const auto &entry : categories) {
        if (library.size() >= 512 || ++count > 64)
          break;
        const auto status = entry.symlink_status(error);
        if (!error && std::filesystem::is_directory(status) &&
            !std::filesystem::is_symlink(status))
          addDirectory(entry.path());
      }
    }
    std::sort(library.begin(), library.end(),
              [](const auto &left, const auto &right) {
                return pathSortKey(left) < pathSortKey(right);
              });
  }

  for (const auto &path : library) {
    if (path != bundled && path != current &&
        std::find(history.begin(), history.end(), path) == history.end() &&
        std::find(favorites.begin(), favorites.end(), path) == favorites.end())
      result.push_back(wallpaperEntry(path, current, false));
  }
  return result;
}

Json wallpaperSnapshot() {
  const auto settings = wallpaperSettings();
  const auto path = activeWallpaperPath(settings);
  auto result = wallpaperMediaStatus();
  result["current"] =
      wallpaperEntry(path, path, path == bundledWallpaperPath());
  auto library = wallpaperLibrarySnapshot();
  const auto favorites = settingPathList(settings, "favorites");
  for (auto &entry : library)
    entry["favorite"] =
        std::find(favorites.begin(), favorites.end(),
                  std::filesystem::path(entry.value("path", std::string{}))) !=
        favorites.end();
  result["library"] = std::move(library);

  const auto directory = std::filesystem::path(
      desktopPreferences().value("wallpaperDirectory", std::string{}));
  std::error_code error;
  const bool exists = std::filesystem::exists(directory, error);
  result["directory"] = directory.string();
  result["resolvedDirectory"] =
      exists ? std::filesystem::canonical(directory, error).string()
             : directory.lexically_normal().string();
  result["directoryExists"] =
      !error && std::filesystem::is_directory(directory, error);
  result["directoryUrl"] = fileUrl(directory);
  return result;
}

bool refreshWallpaperLibrary(std::string *error) {
  ensureLegacyDesktopSettingsImported();
  gchar *uuid = g_uuid_string_random();
  const bool saved = saveWallpaperSettings({{"libraryRefresh", uuid}}, error);
  g_free(uuid);
  return saved;
}

bool createWallpaperDirectory(std::string *error) {
  const auto directory = std::filesystem::path(
      desktopPreferences().value("wallpaperDirectory", std::string{}));
  std::error_code filesystemError;
  if (!directory.is_absolute()) {
    if (error)
      *error = "Could not create the wallpaper library directory.";
    return false;
  }
  std::filesystem::create_directories(directory, filesystemError);
  if (filesystemError) {
    if (error)
      *error = "Could not create the wallpaper library directory.";
    return false;
  }
  return refreshWallpaperLibrary(error);
}

bool setWallpaperFavorite(const std::filesystem::path &path, bool favorite,
                          std::string *error) {
  const auto canonical = canonicalExistingFile(path);
  if (canonical.empty() ||
      (!wallpaperIsVideo(canonical) && !readableImage(canonical))) {
    if (error)
      *error = "The wallpaper is no longer readable.";
    return false;
  }
  auto settings = wallpaperSettings();
  auto favorites = settingPathList(settings, "favorites");
  PathList uniqueFavorites;
  for (const auto &entry : favorites)
    if (std::find(uniqueFavorites.begin(), uniqueFavorites.end(), entry) ==
        uniqueFavorites.end())
      uniqueFavorites.push_back(entry);
  favorites = std::move(uniqueFavorites);
  favorites.erase(std::remove(favorites.begin(), favorites.end(), canonical),
                  favorites.end());
  if (favorite)
    favorites.insert(favorites.begin(), canonical);
  if (favorites.size() > kWallpaperFavoriteLimit)
    favorites.resize(kWallpaperFavoriteLimit);
  if (!saveWallpaperSettings({{"favorites", toJson(favorites)}}, error))
    return false;
  return true;
}

bool selectRandomWallpaper(std::string *error) {
  auto candidates = wallpaperLibrarySnapshot();
  candidates.erase(
      std::remove_if(candidates.begin(), candidates.end(),
                     [](const Json &entry) { return entry.value("current", false); }),
      candidates.end());
  if (candidates.empty()) {
    if (error)
      *error = "Add another image or video to the wallpaper library first.";
    return false;
  }
  const auto index = static_cast<std::size_t>(g_random_int_range(
      0, static_cast<gint>(candidates.size())));
  return setWallpaperImage(candidates[index].value("path", std::string{}),
                           error);
}

std::uint64_t wallpaperRevision() {
  return wallpaperSettings().value("revision", std::uint64_t{0});
}

bool setWallpaperImage(const std::filesystem::path &path, std::string *error) {
  const auto canonicalPath = canonicalExistingFile(path);
  std::error_code filesystemError;
  const auto fileSize = std::filesystem::file_size(canonicalPath,
                                                   filesystemError);
  const bool video = wallpaperIsVideo(canonicalPath);
  bool validImage = false;
  if (!video && !filesystemError && fileSize <= 64 * 1024 * 1024) {
    int width = 0;
    int height = 0;
    validImage = gdk_pixbuf_get_file_info(canonicalPath.c_str(), &width,
                                          &height) != nullptr &&
                 width > 0 && height > 0 &&
                 static_cast<std::uint64_t>(width) *
                         static_cast<std::uint64_t>(height) <=
                     32 * 1024 * 1024;
  }
  const bool validVideo =
      video && !filesystemError &&
      fileSize <= 2ULL * 1024 * 1024 * 1024 &&
      isVideoContent(canonicalPath);
  if (canonicalPath.empty() || filesystemError ||
      (video ? !validVideo : !validImage)) {
    if (error)
      *error = "Choose an image below 64 MiB / 32 megapixels or a local video "
               "below 2 GiB.";
    return false;
  }

  auto settings = wallpaperSettings();
  const auto previous = activeWallpaperPath(settings);
  if (!previous.empty() && previous != canonicalPath)
    rememberWallpaper(settings, previous);
  settings["image"] = canonicalPath.string();
  settings["mode"] = "image";
  rememberWallpaper(settings, canonicalPath);
  bumpWallpaperRevision(settings);
  if (!saveWallpaperSettings({{"image", settings["image"]},
                              {"mode", settings["mode"]},
                              {"history", settings["history"]},
                              {"revision", settings["revision"]}},
                             error)) {
    if (error)
      *error = "Could not save the wallpaper selection: " + *error;
    return false;
  }
  return true;
}

bool resetWallpaperImage(std::string *error) {
  auto settings = wallpaperSettings();
  const auto previous = activeWallpaperPath(settings);
  const auto bundled = bundledWallpaperPath();
  if (!previous.empty() && previous != bundled)
    rememberWallpaper(settings, previous);
  settings["image"] = "";
  settings["mode"] = "image";
  bumpWallpaperRevision(settings);
  return saveWallpaperSettings({{"image", settings["image"]},
                                {"mode", settings["mode"]},
                                {"history", settings["history"]},
                                {"revision", settings["revision"]}},
                               error);
}
} // namespace LunaDash
