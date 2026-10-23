#pragma once

#include <nlohmann/json.hpp>
#include <wayland-server-core.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

namespace LunaDash {

bool isNewerRelease(std::string_view latest, std::string_view current);

class UpdateChecker final {
public:
  using ChannelProvider = std::function<std::string()>;

  UpdateChecker(wl_event_loop *eventLoop, ChannelProvider channelProvider);
  ~UpdateChecker();

  UpdateChecker(const UpdateChecker &) = delete;
  UpdateChecker &operator=(const UpdateChecker &) = delete;

  nlohmann::json snapshot() const;
  void check();

private:
  struct HttpResult {
    bool success = false;
    std::string effectiveUrl;
    std::string body;
  };

  static int handleCompletion(int fd, std::uint32_t mask, void *data);
  static HttpResult fetch(std::string url, std::string userAgent,
                          std::stop_token stop);
  void finishRequest(std::uint64_t generation, const std::string &channel,
                     const HttpResult &result);

  ChannelProvider channelProvider_;
  nlohmann::json state_;
  std::string channel_ = "stable";
  std::string currentCommit_ = "unknown";
  std::jthread requestThread_;
  std::mutex completionMutex_;
  std::optional<HttpResult> completion_;
  std::string completionChannel_;
  int completionFd_ = -1;
  wl_event_source *completionSource_ = nullptr;
  std::uint64_t requestGeneration_ = 0;
  std::uint64_t completionGeneration_ = 0;
  bool requestRunning_ = false;
};

} // namespace LunaDash
