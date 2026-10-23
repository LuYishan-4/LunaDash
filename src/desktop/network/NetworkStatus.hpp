#pragma once

#include "core/process/ProcessRunner.hpp"

#include <nlohmann/json.hpp>
#include <wayland-server-core.h>

#include <functional>
#include <string>
#include <vector>

namespace LunaDash {

nlohmann::json describeNetwork(unsigned int state, unsigned int connectivity);

class NetworkStatus final {
public:
  explicit NetworkStatus(wl_event_loop *eventLoop);
  ~NetworkStatus();

  NetworkStatus(const NetworkStatus &) = delete;
  NetworkStatus &operator=(const NetworkStatus &) = delete;

  nlohmann::json snapshot() const;
  bool execute(const nlohmann::json &request, std::string *error);

private:
  using Completion = ProcessRunner::Completion;

  static int handleRefresh(void *data);
  void refresh();
  void refreshNmcli();
  bool runNmcli(const std::vector<std::string> &arguments,
                Completion completion, int timeoutMilliseconds = 2500);
  void refreshWithoutNmcli();
  void refreshBluetooth();

  nlohmann::json status_;
  ProcessRunner command_;
  ProcessRunner bluetoothCommand_;
  wl_event_source *refreshTimer_ = nullptr;
  std::string nmcli_;
  std::string bluetoothctl_;
  bool pending_ = false;
};

} // namespace LunaDash
