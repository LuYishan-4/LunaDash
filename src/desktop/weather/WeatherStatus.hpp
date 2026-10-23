#pragma once

#include <nlohmann/json.hpp>
#include <wayland-server-core.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace LunaDash {

struct WeatherPreferences {
  bool enabled = false;
  double latitude = 0;
  double longitude = 0;
  std::string location;
};

class WeatherStatus final {
public:
  using PreferencesProvider = WeatherPreferences (*)();

  WeatherStatus(wl_event_loop *eventLoop, PreferencesProvider preferences);
  ~WeatherStatus();

  WeatherStatus(const WeatherStatus &) = delete;
  WeatherStatus &operator=(const WeatherStatus &) = delete;

  nlohmann::json snapshot() const;
  void refresh();

private:
  struct HttpResult {
    bool success = false;
    std::string output;
  };

  static int handleRefresh(void *data);
  static int handleCompletion(int fd, std::uint32_t mask, void *data);
  static HttpResult fetch(std::string url, std::stop_token stop);
  void finishRequest(std::uint64_t generation, bool success,
                     const std::string &output);

  nlohmann::json state_;
  PreferencesProvider preferences_ = nullptr;
  std::jthread requestThread_;
  std::mutex completionMutex_;
  std::optional<HttpResult> completion_;
  int completionFd_ = -1;
  wl_event_source *completionSource_ = nullptr;
  wl_event_source *refreshTimer_ = nullptr;
  std::string configuration_;
  std::uint64_t requestGeneration_ = 0;
  std::uint64_t completionGeneration_ = 0;
  bool requestRunning_ = false;
};

} // namespace LunaDash
