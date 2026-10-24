#pragma once

// Child-process helpers for native tests: run a program with a modified
// environment, collect its output and wait with a deadline.

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <map>
#include <poll.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern char **environ;

namespace LunaDash::Test {

struct ProcessOutput {
  bool started = false;
  bool timedOut = false;
  int exitCode = -1;
  std::string standardOutput;
  std::string standardError;
};

inline std::filesystem::path executableDirectory() {
  std::error_code error;
  return std::filesystem::read_symlink("/proc/self/exe", error).parent_path();
}

inline std::string findExecutable(const std::string &name) {
  const char *path = std::getenv("PATH");
  std::string directories = path ? path : "/usr/bin:/bin";
  std::size_t start = 0;
  while (start <= directories.size()) {
    const auto end = directories.find(':', start);
    const auto directory = directories.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    const auto candidate = std::filesystem::path(directory.empty() ? "." : directory) / name;
    if (::access(candidate.c_str(), X_OK) == 0)
      return candidate.string();
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return {};
}

using Environment = std::map<std::string, std::string>;

inline Environment currentEnvironment() {
  Environment result;
  for (char **entry = environ; entry && *entry; ++entry) {
    const std::string text(*entry);
    const auto equals = text.find('=');
    if (equals != std::string::npos)
      result[text.substr(0, equals)] = text.substr(equals + 1);
  }
  return result;
}

class ChildProcess final {
public:
  ChildProcess() = default;
  ChildProcess(const ChildProcess &) = delete;
  ChildProcess &operator=(const ChildProcess &) = delete;
  ~ChildProcess() { terminate(); }

  bool start(const std::string &program, const std::vector<std::string> &arguments,
             const Environment &environment = currentEnvironment()) {
    int output[2];
    int errors[2];
    if (::pipe2(output, O_CLOEXEC) != 0)
      return false;
    if (::pipe2(errors, O_CLOEXEC) != 0) {
      ::close(output[0]);
      ::close(output[1]);
      return false;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, output[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, errors[1], STDERR_FILENO);
    std::vector<std::string> argumentStorage{program};
    argumentStorage.insert(argumentStorage.end(), arguments.begin(), arguments.end());
    std::vector<char *> argv;
    for (auto &argument : argumentStorage)
      argv.push_back(argument.data());
    argv.push_back(nullptr);
    std::vector<std::string> environmentStorage;
    for (const auto &[key, value] : environment)
      environmentStorage.push_back(key + "=" + value);
    std::vector<char *> envp;
    for (auto &entry : environmentStorage)
      envp.push_back(entry.data());
    envp.push_back(nullptr);
    const int result = ::posix_spawnp(&pid_, program.c_str(), &actions, nullptr,
                                      argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    ::close(output[1]);
    ::close(errors[1]);
    if (result != 0) {
      ::close(output[0]);
      ::close(errors[0]);
      pid_ = -1;
      return false;
    }
    output_ = output[0];
    errors_ = errors[0];
    return true;
  }

  // Reads standard output until a newline arrives or the deadline passes.
  std::string readLine(int timeoutMilliseconds) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMilliseconds);
    while (true) {
      const auto newline = pendingOutput_.find('\n');
      if (newline != std::string::npos) {
        auto line = pendingOutput_.substr(0, newline);
        pendingOutput_.erase(0, newline + 1);
        return line;
      }
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 deadline - std::chrono::steady_clock::now())
                                 .count();
      if (remaining <= 0 || !readSome(output_, pendingOutput_, static_cast<int>(remaining)))
        return {};
    }
  }

  ProcessOutput wait(int timeoutMilliseconds) {
    ProcessOutput result;
    result.started = pid_ > 0;
    if (pid_ <= 0)
      return result;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMilliseconds);
    int status = 0;
    while (true) {
      drain();
      const pid_t waited = ::waitpid(pid_, &status, WNOHANG);
      if (waited == pid_)
        break;
      if (std::chrono::steady_clock::now() >= deadline) {
        result.timedOut = true;
        ::kill(pid_, SIGKILL);
        ::waitpid(pid_, &status, 0);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    pid_ = -1;
    drain();
    while (readSome(output_, pendingOutput_, 0)) {
    }
    while (readSome(errors_, pendingErrors_, 0)) {
    }
    closeDescriptors();
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result.standardOutput = std::move(pendingOutput_);
    result.standardError = std::move(pendingErrors_);
    return result;
  }

  void terminate() {
    if (pid_ > 0) {
      ::kill(pid_, SIGTERM);
      int status = 0;
      for (int attempt = 0; attempt < 200; ++attempt) {
        if (::waitpid(pid_, &status, WNOHANG) == pid_) {
          pid_ = -1;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      if (pid_ > 0) {
        ::kill(pid_, SIGKILL);
        ::waitpid(pid_, &status, 0);
        pid_ = -1;
      }
    }
    closeDescriptors();
  }

  bool running() {
    if (pid_ <= 0)
      return false;
    int status = 0;
    if (::waitpid(pid_, &status, WNOHANG) == pid_) {
      pid_ = -1;
      return false;
    }
    return true;
  }

private:
  static bool readSome(int descriptor, std::string &target, int timeoutMilliseconds) {
    if (descriptor < 0)
      return false;
    pollfd entry{descriptor, POLLIN, 0};
    const int ready = ::poll(&entry, 1, timeoutMilliseconds);
    if (ready <= 0)
      return false;
    char buffer[4096];
    const auto count = ::read(descriptor, buffer, sizeof buffer);
    if (count <= 0)
      return false;
    target.append(buffer, static_cast<std::size_t>(count));
    return true;
  }
  void drain() {
    while (readSome(output_, pendingOutput_, 0)) {
    }
    while (readSome(errors_, pendingErrors_, 0)) {
    }
  }
  void closeDescriptors() {
    if (output_ >= 0)
      ::close(output_);
    if (errors_ >= 0)
      ::close(errors_);
    output_ = errors_ = -1;
  }

  pid_t pid_ = -1;
  int output_ = -1;
  int errors_ = -1;
  std::string pendingOutput_;
  std::string pendingErrors_;
};

inline ProcessOutput runProcess(const std::string &program,
                                const std::vector<std::string> &arguments,
                                const Environment &environment,
                                int timeoutMilliseconds) {
  ChildProcess process;
  if (!process.start(program, arguments, environment))
    return {};
  return process.wait(timeoutMilliseconds);
}

} // namespace LunaDash::Test
