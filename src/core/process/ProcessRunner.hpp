#pragma once

#include "core/process/ProcessEnvironment.hpp"

#include <wayland-server-core.h>

#include <chrono>
#include <functional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace LunaDash {

struct ProcessResult {
  bool success = false;
  bool exited = false;
  int exitCode = -1;
  int signal = 0;
  std::string output;
};

class ProcessRunner final {
public:
  using Completion = std::function<void(bool, const std::string &)>;
  using DetailedCompletion = std::function<void(const ProcessResult &)>;

  explicit ProcessRunner(wl_event_loop *eventLoop, bool collectOutput = true,
                         bool forwardOutput = false);
  ~ProcessRunner();

  ProcessRunner(const ProcessRunner &) = delete;
  ProcessRunner &operator=(const ProcessRunner &) = delete;

  bool run(const std::string &program,
           const std::vector<std::string> &arguments, Completion completion,
           int timeoutMilliseconds = 2500);
  bool run(const std::string &program,
           const std::vector<std::string> &arguments, Completion completion,
           int timeoutMilliseconds, const ProcessEnvironment &environment);
  bool runDetailed(const std::string &program,
                   const std::vector<std::string> &arguments,
                   DetailedCompletion completion,
                   int timeoutMilliseconds = 2500);
  bool runDetailed(const std::string &program,
                   const std::vector<std::string> &arguments,
                   DetailedCompletion completion, int timeoutMilliseconds,
                   const ProcessEnvironment &environment);
  bool busy() const noexcept;
  pid_t processId() const noexcept;
  void cancel() noexcept;

private:
  static int handleOutput(int fd, uint32_t mask, void *data);
  static int handleTimer(void *data);
  void drainOutput(uint32_t mask);
  void finish(bool success, bool hasStatus = false, int status = 0);
  void stopSources();
  void terminateChild();
  bool runWithEnvironment(const std::string &program,
                          const std::vector<std::string> &arguments,
                          DetailedCompletion completion, int timeoutMilliseconds,
                          const ProcessEnvironment *environment);

  wl_event_loop *eventLoop_ = nullptr;
  const bool collectOutput_ = true;
  const bool forwardOutput_ = false;
  wl_event_source *outputSource_ = nullptr;
  wl_event_source *timerSource_ = nullptr;
  int outputFd_ = -1;
  pid_t childPid_ = -1;
  bool busy_ = false;
  bool failed_ = false;
  int timerIntervalMilliseconds_ = 10;
  std::chrono::steady_clock::time_point deadline_{};
  std::string output_;
  DetailedCompletion completion_;
};

} // namespace LunaDash
