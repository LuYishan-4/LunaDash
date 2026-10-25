#include "core/process/ProcessRunner.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool waitFor(wl_event_loop *eventLoop, bool &completed, int timeoutMs = 3000) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (!completed && std::chrono::steady_clock::now() < deadline) {
    if (wl_event_loop_dispatch(eventLoop, 20) < 0)
      return false;
  }
  return completed;
}
} // namespace

int main() {
  wl_event_loop *eventLoop = wl_event_loop_create();
  if (!eventLoop) {
    std::cerr << "Could not create a Wayland event loop.\n";
    return 1;
  }

  int result = 0;
  {
    LunaDash::ProcessRunner runner(eventLoop);
    bool completed = false;
    bool success = false;
    std::string output;
    if (!runner.run("printf", {"runner-output"},
                    [&](bool ok, const std::string &text) {
                      success = ok;
                      output = text;
                      completed = true;
                    }) ||
        !waitFor(eventLoop, completed) || !success ||
        output != "runner-output") {
      std::cerr << "Successful process output was not preserved.\n";
      result = 1;
    }

    completed = false;
    success = false;
    output.clear();
    const char *path = std::getenv("PATH");
    const LunaDash::ProcessEnvironment environment{
        {"PATH", path ? path : "/usr/bin:/bin"},
        {"LUNADASH_PROCESS_RUNNER_TEST", "explicit-environment"}};
    if (!runner.run(
            "/usr/bin/env", {},
            [&](bool ok, const std::string &text) {
              success = ok;
              output = text;
              completed = true;
            },
            2500, environment) ||
        !waitFor(eventLoop, completed) || !success ||
        output.find("LUNADASH_PROCESS_RUNNER_TEST=explicit-environment") ==
            std::string::npos ||
        output.find("HOME=") != std::string::npos) {
      std::cerr << "Explicit process environment was not honored.\n";
      result = 1;
    }

    // Parsed output comes from the C locale; programs whose output is not collected keep
    // the caller's locale (the session shell and applications need their translations).
    completed = false;
    success = false;
    output.clear();
    const LunaDash::ProcessEnvironment localized{
        {"PATH", path ? path : "/usr/bin:/bin"}, {"LANG", "zh_TW.UTF-8"},
        {"LC_ALL", "zh_TW.UTF-8"}};
    if (!runner.run(
            "/usr/bin/env", {},
            [&](bool ok, const std::string &text) {
              success = ok;
              output = text;
              completed = true;
            },
            2500, localized) ||
        !waitFor(eventLoop, completed) || !success ||
        output.find("LC_ALL=C\n") == std::string::npos ||
        output.find("LC_ALL=zh_TW") != std::string::npos) {
      std::cerr << "Collected output was not produced in the C locale.\n";
      result = 1;
    }
    {
      const std::string envFile = "/tmp/lunadash-process-runner-locale-" +
                                  std::to_string(::getpid());
      LunaDash::ProcessRunner launcher(eventLoop, false);
      completed = false;
      if (!launcher.run(
              "/bin/sh", {"-c", "env > \"$0\"", envFile},
              [&](bool, const std::string &) { completed = true; }, 2500,
              localized) ||
          !waitFor(eventLoop, completed)) {
        std::cerr << "A process without collected output did not run.\n";
        result = 1;
      } else {
        std::ifstream file(envFile);
        const std::string written((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        if (written.find("LC_ALL=zh_TW.UTF-8") == std::string::npos ||
            written.find("LC_ALL=C\n") != std::string::npos) {
          std::cerr << "A launched program lost the caller's locale.\n";
          result = 1;
        }
      }
      std::remove(envFile.c_str());
    }

    completed = false;
    success = true;
    output.clear();
    if (!runner.run("false", {},
                    [&](bool ok, const std::string &text) {
                      success = ok;
                      output = text;
                      completed = true;
                    }) ||
        !waitFor(eventLoop, completed) || success) {
      std::cerr << "Nonzero exit status was reported as successful.\n";
      result = 1;
    }

    completed = false;
    LunaDash::ProcessResult processResult;
    if (!runner.runDetailed(
            "sh", {"-c", "printf detailed-output; exit 7"},
            [&](const LunaDash::ProcessResult &value) {
              processResult = value;
              completed = true;
            }) ||
        !waitFor(eventLoop, completed) || processResult.success ||
        !processResult.exited || processResult.exitCode != 7 ||
        processResult.signal != 0 ||
        processResult.output != "detailed-output") {
      std::cerr << "Detailed process exit status was not preserved.\n";
      result = 1;
    }

    completed = false;
    success = true;
    if (!runner.run(
            "sleep", {"2"},
            [&](bool ok, const std::string &) {
              success = ok;
              completed = true;
            },
            40) ||
        runner.run("true", {}, [](bool, const std::string &) {}) ||
        !waitFor(eventLoop, completed) || success) {
      std::cerr << "Timeout or single-process ownership failed.\n";
      result = 1;
    }

    completed = false;
    success = true;
    if (!runner.run(
            "sleep", {"2"},
            [&](bool ok, const std::string &) {
              success = ok;
              completed = true;
            },
            0)) {
      std::cerr << "A cancellable process did not start.\n";
      result = 1;
    } else {
      runner.cancel();
      if (!waitFor(eventLoop, completed) || success || runner.busy()) {
        std::cerr << "Process cancellation did not complete asynchronously.\n";
        result = 1;
      }
    }

    completed = false;
    success = true;
    if (!runner.run(
            "yes", {},
            [&](bool ok, const std::string &text) {
              success = ok;
              output = text;
              completed = true;
            },
            2000) ||
        !waitFor(eventLoop, completed) || success || output.size() > 65536) {
      std::cerr << "The bounded output policy failed.\n";
      result = 1;
    }

    completed = false;
    success = false;
    {
      LunaDash::ProcessRunner quietRunner(eventLoop, false);
      if (!quietRunner.run(
              "yes", {},
              [&](bool ok, const std::string &text) {
                success = ok;
                output = text;
                completed = true;
              },
              40) ||
          !waitFor(eventLoop, completed) || success || !output.empty()) {
        std::cerr
            << "Discarded process output was buffered or timed out late.\n";
        result = 1;
      }
    }

    completed = false;
    success = false;
    if (!runner.run(
            "true", {},
            [&](bool ok, const std::string &) {
              success = ok;
              completed = true;
            },
            0) ||
        !waitFor(eventLoop, completed) || !success) {
      std::cerr << "A process without a deadline did not complete.\n";
      result = 1;
    }

    if (runner.run("/path/that/does/not/exist", {},
                   [](bool, const std::string &) {})) {
      std::cerr << "A missing executable was accepted.\n";
      result = 1;
    }
  }

  wl_event_loop_destroy(eventLoop);
  if (result == 0)
    std::cout << "Native process runner success, failure, timeout and output "
                 "limits passed.\n";
  return result;
}
