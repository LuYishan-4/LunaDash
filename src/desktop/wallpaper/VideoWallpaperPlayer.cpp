#include "desktop/wallpaper/VideoWallpaperPlayer.hpp"

#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"

#include <glib.h>

namespace LunaDash {
namespace {
constexpr int kMaximumFailures = 3;

std::string mpvpaperExecutable() {
  gchar *program = g_find_program_in_path("mpvpaper");
  if (!program)
    return {};
  std::string result(program);
  g_free(program);
  return result;
}
} // namespace

VideoWallpaperPlayer::VideoWallpaperPlayer(wl_event_loop *eventLoop)
    : process_(std::make_unique<ProcessRunner>(eventLoop, false, true)) {}

bool VideoWallpaperPlayer::available() const {
  return !mpvpaperExecutable().empty();
}

pid_t VideoWallpaperPlayer::processId() const {
  return process_->busy() ? process_->processId() : -1;
}

bool VideoWallpaperPlayer::playing() const { return process_->busy(); }

void VideoWallpaperPlayer::cancel() {
  ++generation_;
  process_->cancel();
}

void VideoWallpaperPlayer::stop() {
  cancel();
  source_.clear();
  failures_ = 0;
}

void VideoWallpaperPlayer::sync(const std::filesystem::path &wallpaper,
                                const ProcessEnvironment &environment) {
  if (wallpaper.empty() || !wallpaperIsVideo(wallpaper)) {
    if (!source_.empty())
      stop();
    error_.clear();
    return;
  }
  if (wallpaper != source_) {
    cancel();
    source_ = wallpaper;
    failures_ = 0;
  }
  if (process_->busy() || failures_ >= kMaximumFailures)
    return;
  // A cancelled player is reaped shortly; the next sync starts the new one.
  // A crashed player is restarted, but not faster than every few seconds.
  if (failures_ > 0 &&
      std::chrono::steady_clock::now() - lastStart_ < std::chrono::seconds(3))
    return;
  start(environment);
}

void VideoWallpaperPlayer::start(const ProcessEnvironment &environment) {
  const auto program = mpvpaperExecutable();
  if (program.empty()) {
    error_ = "Install mpvpaper to play video wallpapers.";
    failures_ = kMaximumFailures;
    return;
  }
  // The 1080p proxy LunaDash prepares with ffmpeg decodes more cheaply.
  auto playback = wallpaperPlaybackPath(source_);
  if (playback.empty())
    playback = source_;
  error_.clear();
  lastStart_ = std::chrono::steady_clock::now();
  const bool started = process_->runDetailed(
      program,
      {"--auto-pause", "--layer", "background", "-o",
       "no-audio loop-file=inf panscan=1.0 hwdec=auto no-input-default-bindings",
       "ALL", playback.string()},
      [this, generation = generation_](const ProcessResult &) {
        // Stopped on purpose, or replaced by another wallpaper.
        if (generation != generation_)
          return;
        ++failures_;
        error_ = "mpvpaper stopped playing the video wallpaper.";
      },
      0, environment);
  if (!started) {
    ++failures_;
    error_ = "Could not start mpvpaper.";
  }
}

} // namespace LunaDash
