#pragma once

#include "core/process/ProcessRunner.hpp"

#include <nlohmann/json.hpp>
#include <wayland-server-core.h>

#include <string>
#include <string_view>

namespace LunaDash {

nlohmann::json parsePowerProfiles(std::string_view text);

class PowerSettings final {
public:
  explicit PowerSettings(wl_event_loop *eventLoop);
  ~PowerSettings();

  PowerSettings(const PowerSettings &) = delete;
  PowerSettings &operator=(const PowerSettings &) = delete;

  nlohmann::json snapshot() const;
  bool apply(std::string_view profile, std::string *error);

private:
  static int handleRefresh(void *data);
  void refresh();

  ProcessRunner probe_;
  ProcessRunner action_;
  wl_event_source *refreshTimer_ = nullptr;
  std::string executable_;
  std::string error_;
  nlohmann::json data_;
};

} // namespace LunaDash
