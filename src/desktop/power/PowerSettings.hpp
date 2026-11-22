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

  PowerSettings(const PowerSettings &) = delete;
  PowerSettings &operator=(const PowerSettings &) = delete;

  nlohmann::json snapshot() const;
  bool apply(std::string_view profile, std::string *error);

  // Reads the current values again (they arrive when the probe ends). Nothing
  // polls: the status command asks, as do changes made here.
  void refresh();

private:
  ProcessRunner probe_;
  ProcessRunner action_;
  std::string executable_;
  std::string error_;
  nlohmann::json data_;
};

} // namespace LunaDash
