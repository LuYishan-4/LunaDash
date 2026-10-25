#include "core/process/ProcessRunner.hpp"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <string_view>
#include <utility>

extern char **environ;

namespace LunaDash {
namespace {
constexpr std::size_t kMaximumOutput = 64 * 1024;
constexpr std::size_t kMaximumDrainPerDispatch = 64 * 1024;
constexpr int kReapIntervalMilliseconds = 10;

bool containsNullByte(const std::string &value) {
  return value.find('\0') != std::string::npos;
}

void closeDescriptor(int &fd) {
  if (fd < 0)
    return;
  ::close(fd);
  fd = -1;
}
} // namespace

ProcessRunner::ProcessRunner(wl_event_loop *eventLoop, bool collectOutput,
                             bool forwardOutput)
    : eventLoop_(eventLoop), collectOutput_(collectOutput),
      forwardOutput_(forwardOutput) {}

ProcessRunner::~ProcessRunner() {
  completion_ = {};
  stopSources();
  terminateChild();
  if (childPid_ > 0) {
    int status = 0;
    while (::waitpid(childPid_, &status, 0) < 0 && errno == EINTR) {
    }
    childPid_ = -1;
  }
}

bool ProcessRunner::run(const std::string &program,
                        const std::vector<std::string> &arguments,
                        Completion completion, int timeoutMilliseconds) {
  if (!completion)
    return false;
  return runDetailed(
      program, arguments,
      [completion = std::move(completion)](const ProcessResult &result) {
        completion(result.success, result.output);
      },
      timeoutMilliseconds);
}

bool ProcessRunner::run(const std::string &program,
                        const std::vector<std::string> &arguments,
                        Completion completion, int timeoutMilliseconds,
                        const ProcessEnvironment &environment) {
  if (!completion)
    return false;
  return runDetailed(
      program, arguments,
      [completion = std::move(completion)](const ProcessResult &result) {
        completion(result.success, result.output);
      },
      timeoutMilliseconds, environment);
}

bool ProcessRunner::runDetailed(const std::string &program,
                                const std::vector<std::string> &arguments,
                                DetailedCompletion completion,
                                int timeoutMilliseconds) {
  return runWithEnvironment(program, arguments, std::move(completion),
                            timeoutMilliseconds, nullptr);
}

bool ProcessRunner::runDetailed(const std::string &program,
                                const std::vector<std::string> &arguments,
                                DetailedCompletion completion,
                                int timeoutMilliseconds,
                                const ProcessEnvironment &environment) {
  return runWithEnvironment(program, arguments, std::move(completion),
                            timeoutMilliseconds, &environment);
}

bool ProcessRunner::runWithEnvironment(
    const std::string &program, const std::vector<std::string> &arguments,
    DetailedCompletion completion, int timeoutMilliseconds,
    const ProcessEnvironment *environmentOverride) {
  if (busy_ || !eventLoop_ || program.empty() || containsNullByte(program) ||
      !completion ||
      std::any_of(arguments.begin(), arguments.end(), containsNullByte))
    return false;
  if (environmentOverride)
    for (const auto &[name, value] : *environmentOverride)
      if (name.empty() || name.find('=') != std::string::npos ||
          containsNullByte(name) || containsNullByte(value))
        return false;

  int pipeFds[2] = {-1, -1};
  if (!forwardOutput_) {
    if (::pipe2(pipeFds, O_CLOEXEC) != 0)
      return false;
    const int readFlags = ::fcntl(pipeFds[0], F_GETFL, 0);
    if (readFlags < 0 ||
        ::fcntl(pipeFds[0], F_SETFL, readFlags | O_NONBLOCK) < 0) {
      closeDescriptor(pipeFds[0]);
      closeDescriptor(pipeFds[1]);
      return false;
    }
  }

  std::vector<std::string> argumentStorage;
  argumentStorage.reserve(arguments.size() + 1);
  argumentStorage.push_back(program);
  argumentStorage.insert(argumentStorage.end(), arguments.begin(),
                         arguments.end());
  std::vector<char *> argv;
  argv.reserve(argumentStorage.size() + 1);
  for (auto &argument : argumentStorage)
    argv.push_back(argument.data());
  argv.push_back(nullptr);

  // Output that is parsed comes from the C locale, so it is stable English. Programs whose
  // output is not collected (the session shell, desktop applications) keep the user's
  // locale, or they would lose their translations.
  const bool parsedOutput = collectOutput_;
  std::vector<std::string> environmentStorage;
  if (environmentOverride) {
    for (const auto &[name, value] : *environmentOverride) {
      if (parsedOutput && name == "LC_ALL")
        continue;
      environmentStorage.push_back(name + "=" + value);
    }
  } else {
    for (char **entry = environ; entry && *entry; ++entry) {
      if (parsedOutput && std::string_view(*entry).starts_with("LC_ALL="))
        continue;
      environmentStorage.emplace_back(*entry);
    }
  }
  if (parsedOutput)
    environmentStorage.emplace_back("LC_ALL=C");
  std::vector<char *> environment;
  environment.reserve(environmentStorage.size() + 1);
  for (auto &entry : environmentStorage)
    environment.push_back(entry.data());
  environment.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  int spawnError = ::posix_spawn_file_actions_init(&actions);
  const bool actionsInitialized = spawnError == 0;
  if (spawnError == 0 && !forwardOutput_)
    spawnError =
        ::posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
  if (spawnError == 0 && !forwardOutput_)
    spawnError =
        ::posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDERR_FILENO);
  if (spawnError == 0 && !forwardOutput_ && pipeFds[0] != STDOUT_FILENO &&
      pipeFds[0] != STDERR_FILENO)
    spawnError = ::posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
  if (spawnError == 0 && !forwardOutput_ && pipeFds[1] != STDOUT_FILENO &&
      pipeFds[1] != STDERR_FILENO)
    spawnError = ::posix_spawn_file_actions_addclose(&actions, pipeFds[1]);

  pid_t child = -1;
  if (spawnError == 0)
    spawnError = ::posix_spawnp(&child, program.c_str(), &actions, nullptr,
                                argv.data(), environment.data());
  if (actionsInitialized)
    ::posix_spawn_file_actions_destroy(&actions);
  closeDescriptor(pipeFds[1]);
  if (spawnError != 0) {
    closeDescriptor(pipeFds[0]);
    return false;
  }

  childPid_ = child;
  outputFd_ = pipeFds[0];
  output_.clear();
  failed_ = false;
  busy_ = true;
  completion_ = std::move(completion);
  deadline_ = timeoutMilliseconds <= 0
                  ? std::chrono::steady_clock::time_point::max()
                  : std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(
                            std::clamp(timeoutMilliseconds, 1, 60000));
  timerIntervalMilliseconds_ =
      timeoutMilliseconds <= 0 ? 100 : kReapIntervalMilliseconds;

  if (outputFd_ >= 0)
    outputSource_ = wl_event_loop_add_fd(
        eventLoop_, outputFd_,
        WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR, handleOutput,
        this);
  timerSource_ = wl_event_loop_add_timer(eventLoop_, handleTimer, this);
  if ((!forwardOutput_ && !outputSource_) || !timerSource_ ||
      wl_event_source_timer_update(timerSource_, timerIntervalMilliseconds_) <
          0) {
    failed_ = true;
    terminateChild();
    int status = 0;
    pid_t waited = -1;
    do {
      waited = ::waitpid(childPid_, &status, 0);
    } while (waited < 0 && errno == EINTR);
    childPid_ = -1;
    finish(false, waited > 0, status);
    return true;
  }
  return true;
}

bool ProcessRunner::busy() const noexcept { return busy_; }

pid_t ProcessRunner::processId() const noexcept { return childPid_; }

void ProcessRunner::cancel() noexcept {
  if (!busy_)
    return;
  failed_ = true;
  terminateChild();
  if (timerSource_)
    wl_event_source_timer_update(timerSource_, kReapIntervalMilliseconds);
}

int ProcessRunner::handleOutput(int, uint32_t mask, void *data) {
  auto *runner = static_cast<ProcessRunner *>(data);
  runner->drainOutput(mask);
  return 0;
}

int ProcessRunner::handleTimer(void *data) {
  auto *runner = static_cast<ProcessRunner *>(data);
  int status = 0;
  const pid_t result = ::waitpid(runner->childPid_, &status, WNOHANG);
  if (result == runner->childPid_) {
    runner->childPid_ = -1;
    runner->drainOutput(0);
    const bool success =
        !runner->failed_ && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    runner->finish(success, true, status);
    return 0;
  }
  if (result < 0 && errno != EINTR) {
    runner->failed_ = true;
    runner->childPid_ = -1;
    runner->finish(false);
    return 0;
  }
  if (runner->deadline_ != std::chrono::steady_clock::time_point::max() &&
      std::chrono::steady_clock::now() >= runner->deadline_) {
    runner->failed_ = true;
    runner->terminateChild();
  }
  if (runner->timerSource_)
    wl_event_source_timer_update(runner->timerSource_,
                                 runner->timerIntervalMilliseconds_);
  return 0;
}

void ProcessRunner::drainOutput(uint32_t mask) {
  if (outputFd_ < 0)
    return;

  std::array<char, 4096> buffer{};
  bool closePipe = false;
  std::size_t drained = 0;
  for (;;) {
    const ssize_t count = ::read(outputFd_, buffer.data(), buffer.size());
    if (count > 0) {
      const auto received = static_cast<std::size_t>(count);
      drained += received;
      if (collectOutput_) {
        const auto available = kMaximumOutput - output_.size();
        output_.append(buffer.data(), std::min(available, received));
        if (received > available) {
          failed_ = true;
          terminateChild();
          break;
        }
      }
      if (drained >= kMaximumDrainPerDispatch)
        break;
      continue;
    }
    if (count == 0) {
      closePipe = true;
      break;
    }
    if (errno == EINTR)
      continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      closePipe = (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) != 0;
      if (mask & WL_EVENT_ERROR) {
        failed_ = true;
        terminateChild();
      }
      break;
    }
    failed_ = true;
    terminateChild();
    closePipe = true;
    break;
  }

  if (closePipe) {
    if (outputSource_) {
      wl_event_source_remove(outputSource_);
      outputSource_ = nullptr;
    }
    closeDescriptor(outputFd_);
  }
}

void ProcessRunner::finish(bool success, bool hasStatus, int status) {
  stopSources();
  childPid_ = -1;
  busy_ = false;
  failed_ = false;
  auto completion = std::move(completion_);
  completion_ = {};
  ProcessResult result;
  result.success = success;
  if (hasStatus && WIFEXITED(status)) {
    result.exited = true;
    result.exitCode = WEXITSTATUS(status);
  } else if (hasStatus && WIFSIGNALED(status)) {
    result.signal = WTERMSIG(status);
  }
  result.output = std::move(output_);
  output_.clear();
  if (completion)
    completion(result);
}

void ProcessRunner::stopSources() {
  if (outputSource_) {
    wl_event_source_remove(outputSource_);
    outputSource_ = nullptr;
  }
  if (timerSource_) {
    wl_event_source_remove(timerSource_);
    timerSource_ = nullptr;
  }
  closeDescriptor(outputFd_);
}

void ProcessRunner::terminateChild() {
  if (childPid_ > 0)
    ::kill(childPid_, SIGKILL);
}

} // namespace LunaDash
