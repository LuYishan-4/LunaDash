#pragma once

#include "core/process/ProcessRunner.hpp"

#include <nlohmann/json.hpp>
#include <wayland-server-core.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

nlohmann::json parseDdcDisplays(std::string_view text);
nlohmann::json parseDdcBrightness(std::string_view text);

class DdcBrightnessSettings final {
public:
  explicit DdcBrightnessSettings(wl_event_loop *eventLoop);
  ~DdcBrightnessSettings();

  DdcBrightnessSettings(const DdcBrightnessSettings &) = delete;
  DdcBrightnessSettings &operator=(const DdcBrightnessSettings &) = delete;

  nlohmann::json snapshot() const;
  bool setPercent(std::string_view id, int percent, std::string *error);
  void refresh();

private:
  static int handleRefresh(void *data);
  void advance();

  ProcessRunner runner_;
  wl_event_source *refreshTimer_ = nullptr;
  std::string executable_;
  std::string error_;
  std::string activeId_;
  std::vector<nlohmann::json> devices_;
  std::map<std::string, int> pending_;
  bool refreshRequested_ = false;
  bool scanning_ = false;
  std::optional<std::size_t> queryIndex_;
};

} // namespace LunaDash
