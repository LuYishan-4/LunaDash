#pragma once

#include "core/process/ProcessRunner.hpp"

#include <functional>
#include <string>
#include <vector>

namespace LunaDash {

class SessionServices final {
public:
  explicit SessionServices(wl_event_loop *eventLoop);

  SessionServices(const SessionServices &) = delete;
  SessionServices &operator=(const SessionServices &) = delete;

  void publishActivationEnvironment(
      const ProcessEnvironment &environment,
      std::function<void(bool, const std::string &)> completion);
  void refreshScreencastPortalServices(const ProcessEnvironment &environment,
                                       std::function<void()> completion = {});

private:
  bool startSystemctl(const ProcessEnvironment &environment,
                      const std::vector<std::string> &services,
                      std::function<void()> completion);

  ProcessRunner activationProcess_;
  ProcessRunner systemctlProcess_;
};

} // namespace LunaDash
