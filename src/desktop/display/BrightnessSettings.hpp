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

  BrightnessSettings(const BrightnessSettings &) = delete;
  BrightnessSettings &operator=(const BrightnessSettings &) = delete;

  nlohmann::json snapshot() const;
  bool setPercent(int percent, std::string *error);
  // Reads the current values again (they arrive when the probe ends). Nothing
  // polls: the status command asks, as do changes made here.
  void refresh();

private:
  void applyPending();

  ProcessRunner query_;
  ProcessRunner action_;
  std::string executable_;
  std::string error_;
  nlohmann::json data_ = {{"available", false}};
  int pending_ = -1;
};

} // namespace LunaDash
