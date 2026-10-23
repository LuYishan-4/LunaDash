#pragma once

#include "core/process/ProcessRunner.hpp"

#include <functional>
#include <string>
#include <vector>

namespace LunaDash {

class SessionServices final {
public:
  using ActivationCompletion = std::function<void(bool, const std::string &)>;
  using Completion = std::function<void()>;

  explicit SessionServices(wl_event_loop *eventLoop);

  SessionServices(const SessionServices &) = delete;
  SessionServices &operator=(const SessionServices &) = delete;

  void publishActivationEnvironment(const ProcessEnvironment &environment,
                                    ActivationCompletion completion);
  void refreshScreencastPortalServices(const ProcessEnvironment &environment,
                                       Completion completion = {});

private:
  bool startSystemctl(const ProcessEnvironment &environment,
                      const std::vector<std::string> &services,
                      Completion completion);

  ProcessRunner activationProcess_;
  ProcessRunner portalProcess_;
  ProcessRunner systemctlProcess_;
};

} // namespace LunaDash
