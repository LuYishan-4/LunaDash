#pragma once

#include "core/process/ProcessRunner.hpp"
#include "desktop/system/SystemMetrics.h"

#include <nlohmann/json.hpp>
#include <wayland-server-core.h>

#include <string>

namespace LunaDash {

class SystemStatus final {
public:
  explicit SystemStatus(wl_event_loop *eventLoop);
  ~SystemStatus();

  SystemStatus(const SystemStatus &) = delete;
  SystemStatus &operator=(const SystemStatus &) = delete;

  nlohmann::json snapshot() const;

private:
  static int handleRefresh(void *data);
  void refresh();
  void refreshGpu();
  void finishGpuQuery(bool success, const std::string &output);

  nlohmann::json data_;
  wl_event_loop *eventLoop_ = nullptr;
  wl_event_source *refreshTimer_ = nullptr;
  ProcessRunner gpuQuery_;
  LuDashCpuCounters previous_{};
  std::string nvidiaSmi_;
  int refreshCount_ = 0;
};

} // namespace LunaDash
