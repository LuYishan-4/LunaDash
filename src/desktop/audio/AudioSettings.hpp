#pragma once

#include "core/process/ProcessRunner.hpp"

#include <nlohmann/json.hpp>
#include <wayland-server-core.h>

#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

nlohmann::json parseAudioVolume(std::string_view text);
nlohmann::json parseAudioDevices(std::string_view text);
std::vector<std::string> audioCommand(const nlohmann::json &change);

class AudioSettings final {
public:
  explicit AudioSettings(wl_event_loop *eventLoop);

  AudioSettings(const AudioSettings &) = delete;
  AudioSettings &operator=(const AudioSettings &) = delete;

  nlohmann::json snapshot() const;
  bool apply(const nlohmann::json &change, std::string *error);

  // Reads the current values again (they arrive when the probe ends). Nothing
  // polls: the status command asks, as do changes made here.
  void refresh();

private:
  ProcessRunner outputProbe_;
  ProcessRunner inputProbe_;
  ProcessRunner action_;
  ProcessRunner devicesProbe_;
  std::string executable_;
  std::string error_;
  int refreshCount_ = 0;
  nlohmann::json output_;
  nlohmann::json input_;
  nlohmann::json outputDevices_ = nlohmann::json::array();
};

} // namespace LunaDash
