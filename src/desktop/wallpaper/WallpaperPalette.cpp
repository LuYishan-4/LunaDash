#include "desktop/wallpaper/WallpaperPalette.hpp"

#include "config/desktop/DesktopPreferences.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "core/process/ProcessRunner.hpp"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

namespace LunaDash {
namespace {
struct MediaState {
  wl_event_loop *eventLoop = nullptr;
  std::unique_ptr<ProcessRunner> optimizationRunner;
  std::unique_ptr<ProcessRunner> extractionRunner;
  std::filesystem::path selectedPath;
  std::filesystem::path optimizationPath;
  std::filesystem::path optimizationTemporary;
  std::string error;
  std::uint64_t generation = 0;
  std::uint64_t taskGeneration = 0;
  bool selectedVideo = false;
  bool optimizing = false;
  bool extracting = false;
};

MediaState &mediaState() {
  static MediaState state;
  return state;
}

std::filesystem::path cacheStem(const std::filesystem::path &path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  const auto modified = std::filesystem::last_write_time(path, error);
  const std::string identity =
      path.string() + (error ? std::string{} : std::to_string(size)) +
      (error ? std::string{} : std::to_string(modified.time_since_epoch().count()));
  GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
  g_checksum_update(checksum,
                    reinterpret_cast<const guchar *>(identity.data()),
                    identity.size());
  const auto *digest = g_checksum_get_string(checksum);
  const auto result =
      std::filesystem::path(g_get_user_cache_dir()) / "lunadash" /
      "wallpapers" / digest;
  g_checksum_free(checksum);
  return result;
}

std::filesystem::path ensureCacheDirectory(const std::filesystem::path &file,
                                          std::string *error) {
  const auto directory = file.parent_path();
  std::error_code filesystemError;
  std::filesystem::create_directories(directory, filesystemError);
  if (filesystemError) {
    *error = "Could not create the wallpaper preview cache.";
    return {};
  }
  return directory;
}

std::string findExecutable(const char *name) {
  gchar *program = g_find_program_in_path(name);
  if (!program)
    return {};
  std::string result(program);
  g_free(program);
  return result;
}

std::array<double, 3> rgbToHsl(double red, double green, double blue) {
  const double maximum = std::max({red, green, blue});
  const double minimum = std::min({red, green, blue});
  const double lightness = (maximum + minimum) / 2.0;
  if (maximum == minimum)
    return {0.0, 0.0, lightness};

  const double delta = maximum - minimum;
  const double saturation =
      lightness > 0.5 ? delta / (2.0 - maximum - minimum)
                      : delta / (maximum + minimum);
  double hue = 0.0;
  if (maximum == red)
    hue = (green - blue) / delta + (green < blue ? 6.0 : 0.0);
  else if (maximum == green)
    hue = (blue - red) / delta + 2.0;
  else
    hue = (red - green) / delta + 4.0;
  return {hue / 6.0, saturation, lightness};
}

std::string hslHex(double hue, double saturation, double lightness) {
  const auto component = [hue, saturation, lightness](double offset) {
    const double k = std::fmod(offset + hue * 12.0, 12.0);
    const double chroma =
        saturation * std::min(lightness, 1.0 - lightness);
    return lightness -
           chroma * std::max(-1.0, std::min({k - 3.0, 9.0 - k, 1.0}));
  };
  const auto channel = [](double value) {
    return static_cast<unsigned>(std::lround(value * 255.0));
  };
  constexpr char digits[] = "0123456789abcdef";
  std::string color = "#000000";
  const std::array<unsigned, 3> channels{
      channel(component(0.0)), channel(component(8.0 / 12.0)),
      channel(component(4.0 / 12.0))};
  for (std::size_t i = 0; i < channels.size(); ++i) {
    color[1 + i * 2] = digits[(channels[i] >> 4) & 0xf];
    color[2 + i * 2] = digits[channels[i] & 0xf];
  }
  return color;
}

bool extractPalette(const std::filesystem::path &imagePath,
                    const std::filesystem::path &original) {
  if (!desktopPreferences().value("wallpaperColors", true) ||
      wallpaperImagePath() != original)
    return true;

  GError *loadError = nullptr;
  GdkPixbuf *loaded = gdk_pixbuf_new_from_file_at_scale(
      imagePath.c_str(), 96, 96, TRUE, &loadError);
  if (!loaded) {
    if (loadError)
      g_error_free(loadError);
    return false;
  }
  GdkPixbuf *oriented = gdk_pixbuf_apply_embedded_orientation(loaded);
  g_object_unref(loaded);
  GdkPixbuf *image = oriented;

  std::array<double, 24> weights{};
  std::array<double, 24> saturation{};
  const int imageWidth = gdk_pixbuf_get_width(image);
  const int imageHeight = gdk_pixbuf_get_height(image);
  const int channels = gdk_pixbuf_get_n_channels(image);
  const int rowStride = gdk_pixbuf_get_rowstride(image);
  const bool hasAlpha = gdk_pixbuf_get_has_alpha(image);
  const guchar *pixels = gdk_pixbuf_read_pixels(image);
  for (int y = 0; y < imageHeight; ++y) {
    const auto *row = pixels + y * rowStride;
    for (int x = 0; x < imageWidth; ++x) {
      const auto *pixel = row + x * channels;
      const double alpha = hasAlpha ? pixel[3] / 255.0 : 1.0;
      if (alpha < 0.5)
        continue;
      const auto hsl = rgbToHsl(pixel[0] / 255.0, pixel[1] / 255.0,
                               pixel[2] / 255.0);
      if (hsl[2] < 0.08 || hsl[2] > 0.93)
        continue;
      const auto index = std::clamp(static_cast<int>(hsl[0] * 24.0), 0, 23);
      const double weight = 0.05 + hsl[1];
      weights[index] += weight;
      saturation[index] += hsl[1] * weight;
    }
  }
  g_object_unref(image);

  const auto largest = std::max_element(weights.begin(), weights.end());
  if (*largest <= 0)
    return true;
  const int index = static_cast<int>(largest - weights.begin());
  const double hue = (index + 0.5) / 24.0;
  const double sat = std::clamp(saturation[index] / *largest, 0.25, 0.65);
  std::string error;
  if (!updateDesktopPreferences(
          {{"accent", hslHex(hue, sat, 0.78)},
           {"secondaryAccent", hslHex(hue, 0.22, 0.65)}},
          &error)) {
    mediaState().error = std::move(error);
    return false;
  }
  return true;
}

void startMediaTasks();

void finishOptimization(std::uint64_t generation,
                        const std::filesystem::path &source,
                        const std::filesystem::path &proxy,
                        const std::filesystem::path &temporary,
                        const ProcessResult &result) {
  auto &state = mediaState();
  state.optimizing = false;
  std::error_code filesystemError;
  if (generation == state.generation && state.selectedPath == source) {
    const auto size = std::filesystem::file_size(temporary, filesystemError);
    if (result.success && !filesystemError && size > 0) {
      std::filesystem::rename(temporary, proxy, filesystemError);
      if (filesystemError)
        state.error = "Could not publish the optimized wallpaper proxy.";
    } else {
      state.error = "Could not optimize the live wallpaper.";
    }
  }
  if (state.optimizationPath == proxy) {
    state.optimizationPath.clear();
    state.optimizationTemporary.clear();
  }
  std::filesystem::remove(temporary, filesystemError);
  if (generation != state.generation && !state.optimizationRunner->busy() &&
      !state.extractionRunner->busy())
    startMediaTasks();
}

void finishExtraction(std::uint64_t generation,
                      const std::filesystem::path &source,
                      const std::filesystem::path &poster,
                      const ProcessResult &result) {
  auto &state = mediaState();
  state.extracting = false;
  std::error_code filesystemError;
  if (generation == state.generation && state.selectedPath == source) {
    const auto size = std::filesystem::file_size(poster, filesystemError);
    if (result.success && !filesystemError && size > 0) {
      if (!extractPalette(poster, source) && state.error.empty())
        state.error = "Could not decode a preview frame from this video.";
    } else {
      std::filesystem::remove(poster, filesystemError);
      state.error = "Could not decode a preview frame from this video.";
    }
  } else {
    std::filesystem::remove(poster, filesystemError);
  }
  if (generation != state.generation && !state.optimizationRunner->busy() &&
      !state.extractionRunner->busy())
    startMediaTasks();
}

void startMediaTasks() {
  auto &state = mediaState();
  if (!state.eventLoop || state.optimizationRunner->busy() ||
      state.extractionRunner->busy())
    return;
  state.taskGeneration = state.generation;
  state.error.clear();
  const auto source = state.selectedPath;
  if (!state.selectedVideo) {
    if (!extractPalette(source, source) && state.error.empty())
      state.error = "Could not decode the wallpaper image.";
    return;
  }

  const auto poster = wallpaperPoster(source);
  std::error_code filesystemError;
  const bool posterExists =
      std::filesystem::is_regular_file(poster, filesystemError);
  if (posterExists && !extractPalette(poster, source))
    state.error = "Could not decode the wallpaper preview.";

  const auto ffmpeg = findExecutable("ffmpeg");
  if (ffmpeg.empty()) {
    if (!posterExists)
      state.error =
          "Install ffmpeg to generate live wallpaper previews and colors.";
    return;
  }
  const auto proxy = cacheStem(source).string() + "-1080p30.mp4";
  const auto temporary = std::filesystem::path(proxy + ".part.mp4");
  if (!posterExists &&
      ensureCacheDirectory(poster, &state.error).empty())
    return;

  if (!std::filesystem::exists(proxy, filesystemError)) {
    const auto proxyDirectory =
        ensureCacheDirectory(proxy, &state.error);
    if (proxyDirectory.empty())
      return;
    std::filesystem::remove(temporary, filesystemError);
    state.optimizationPath = proxy;
    state.optimizationTemporary = temporary;
    state.optimizing = true;
    if (!state.optimizationRunner->runDetailed(
        ffmpeg,
        {"-nostdin", "-v", "error", "-y", "-i", source.string(), "-an",
         "-vf",
         "scale=w='min(iw,1920)':h='min(ih,1080)':force_original_aspect_ratio=decrease:force_divisible_by=2,fps=30",
         "-c:v", "libx264", "-preset", "veryfast", "-crf", "24", "-pix_fmt",
         "yuv420p", "-movflags", "+faststart", "-threads", "1",
         temporary.string()},
        [generation = state.generation, source, proxy,
         temporary](const ProcessResult &result) {
          finishOptimization(generation, source, proxy, temporary, result);
        },
        0)) {
      state.optimizing = false;
      state.error = "Could not start ffmpeg for the live wallpaper proxy.";
      state.optimizationPath.clear();
      state.optimizationTemporary.clear();
    }
  }

  if (posterExists)
    return;
  state.extracting = true;
  if (!state.extractionRunner->runDetailed(
      ffmpeg,
      {"-nostdin", "-v", "error", "-y", "-protocol_whitelist", "file,pipe",
       "-i", source.string(), "-frames:v", "1", "-vf", "scale=640:-2",
       "-threads", "1", poster.string()},
      [generation = state.generation, source,
       poster](const ProcessResult &result) {
        finishExtraction(generation, source, poster, result);
      },
      12000)) {
    state.extracting = false;
    state.error = "Could not start ffmpeg for the wallpaper preview.";
  }
}
} // namespace

std::filesystem::path wallpaperPoster(const std::filesystem::path &path) {
  return cacheStem(path).string() + ".png";
}

std::filesystem::path
wallpaperPlaybackPath(const std::filesystem::path &path) {
  if (!wallpaperIsVideo(path))
    return path;
  const std::filesystem::path proxy =
      cacheStem(path).string() + "-1080p30.mp4";
  std::error_code error;
  if (std::filesystem::is_regular_file(proxy, error))
    return proxy;
  const auto &state = mediaState();
  return state.optimizing && state.optimizationPath == proxy
             ? std::filesystem::path{}
             : path;
}

nlohmann::json wallpaperMediaStatus() {
  const auto &state = mediaState();
  return {{"extracting", state.extracting},
          {"optimizing", state.optimizing},
          {"optimizedPath", state.optimizationPath.string()},
          {"error", state.error},
          {"ffmpegAvailable", !findExecutable("ffmpeg").empty()}};
}

void refreshWallpaperPalette(wl_event_loop *eventLoop,
                             const std::filesystem::path &path, bool video) {
  auto &state = mediaState();
  if (!eventLoop) {
    state.error = "Wallpaper processing requires the compositor event loop.";
    return;
  }
  if (state.eventLoop && state.eventLoop != eventLoop)
    cancelWallpaperPalette();
  state.eventLoop = eventLoop;
  if (!state.optimizationRunner)
    state.optimizationRunner = std::make_unique<ProcessRunner>(eventLoop, false);
  if (!state.extractionRunner)
    state.extractionRunner = std::make_unique<ProcessRunner>(eventLoop, false);
  state.selectedPath = path;
  state.selectedVideo = video;
  ++state.generation;
  state.error.clear();
  startMediaTasks();
}

void cancelWallpaperPalette() {
  auto &state = mediaState();
  if (state.optimizationRunner) {
    state.optimizationRunner->cancel();
    state.optimizationRunner.reset();
  }
  if (state.extractionRunner) {
    state.extractionRunner->cancel();
    state.extractionRunner.reset();
  }
  std::error_code error;
  if (!state.optimizationTemporary.empty())
    std::filesystem::remove(state.optimizationTemporary, error);
  state.eventLoop = nullptr;
  state.optimizationPath.clear();
  state.optimizationTemporary.clear();
  state.optimizing = false;
  state.extracting = false;
  ++state.generation;
}
} // namespace LunaDash
