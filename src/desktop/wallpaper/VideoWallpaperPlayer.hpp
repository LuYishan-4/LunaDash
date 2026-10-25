#pragma once

#include "core/process/ProcessEnvironment.hpp"
#include "core/process/ProcessRunner.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/types.h>

struct wl_event_loop;

namespace LunaDash {

// Plays a video wallpaper with mpvpaper, a wlr-layer-shell client drawing on the
// background layer. The compositor keeps its surfaces below the shell's
// wallpaper, which turns transparent while the video plays.
class VideoWallpaperPlayer final {
public:
  explicit VideoWallpaperPlayer(wl_event_loop *eventLoop);

  // Plays `wallpaper` when it is a video, or stops. Cheap when nothing changed.
  void sync(const std::filesystem::path &wallpaper,
            const ProcessEnvironment &environment);
  void stop();

  // The running player process, for recognising its surfaces.
  pid_t processId() const;
  bool playing() const;
  // mpvpaper is installed.
  bool available() const;
  // Why a video wallpaper does not play ("" when it does or none is chosen).
  const std::string &error() const { return error_; }

private:
  void start(const ProcessEnvironment &environment);
  // Kills the player; its completion then knows the stop was intended.
  void cancel();

  std::unique_ptr<ProcessRunner> process_;
  std::filesystem::path source_;
  std::string error_;
  int failures_ = 0;
  unsigned generation_ = 0;
  std::chrono::steady_clock::time_point lastStart_{};
};

} // namespace LunaDash
