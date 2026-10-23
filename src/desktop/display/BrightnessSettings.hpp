#pragma once

#include "core/process/ProcessRunner.hpp"

#include <nlohmann/json.hpp>
#include <wayland-server-core.h>

#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

nlohmann::json parseBacklight(std::string_view text);

class BrightnessSettings final {
public:
  explicit BrightnessSettings(wl_event_loop *eventLoop);
  ~BrightnessSettings();

  BrightnessSettings(const BrightnessSettings &) = delete;
  BrightnessSettings &operator=(const BrightnessSettings &) = delete;

  nlohmann::json snapshot() const;
  bool setPercent(int percent, std::string *error);
  void refresh();

private:
  static int handleRefresh(void *data);
  void applyPending();

  ProcessRunner query_;
  ProcessRunner action_;
  wl_event_source *refreshTimer_ = nullptr;
  std::string executable_;
  std::string error_;
  nlohmann::json data_ = {{"available", false}};
  int pending_ = -1;
};

} // namespace LunaDash
