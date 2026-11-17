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

class VideoWallpaperPlayer final {
public:
  explicit VideoWallpaperPlayer(wl_event_loop *eventLoop);

  void sync(const std::filesystem::path &wallpaper,
         const ProcessEnvironment &environment);
  void stop();
  pid_t processId() const;
  bool playing() const;
  bool available() const;
  const std::string &error() const { return error_; }

private:
  void start(const ProcessEnvironment &environment);
  void cancel();

  std::unique_ptr<ProcessRunner> process_;
  std::filesystem::path source_;
  std::string error_;
  int failures_ = 0;
  unsigned generation_ = 0;
  std::chrono::steady_clock::time_point lastStart_{};
};

} // namespace LunaDash
