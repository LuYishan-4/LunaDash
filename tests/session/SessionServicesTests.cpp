#include "compositor/session/SessionServices.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {

void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

class TemporaryTree final {
public:
  TemporaryTree() {
    std::string pattern = "/tmp/lunadash-session-services-XXXXXX";
    std::vector<char> path(pattern.begin(), pattern.end());
    path.push_back('\0');
    char *created = ::mkdtemp(path.data());
    if (!created)
      throw std::runtime_error("Could not create temporary test directory.");
    path_ = created;
  }

  ~TemporaryTree() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

bool waitFor(wl_event_loop *eventLoop, bool &completed,
             int timeoutMilliseconds = 3000) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMilliseconds);
  while (!completed && std::chrono::steady_clock::now() < deadline)
    if (wl_event_loop_dispatch(eventLoop, 20) < 0)
      return false;
  return completed;
}

void writeExecutable(const std::filesystem::path &path,
                     const std::string &contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path);
  output << contents;
  output.close();
  if (!output || ::chmod(path.c_str(), 0755) != 0)
    throw std::runtime_error("Could not create fake service executable.");
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void activationSystemdFailureFallsBackToBusEnvironment(
    SessionServices &services, wl_event_loop *eventLoop,
    const ProcessEnvironment &environment,
    const std::filesystem::path &logPath) {
  std::filesystem::remove(logPath);
  ::setenv("LUDASH_FAKE_ACTIVATION_FAIL_SYSTEMD", "1", 1);
  bool completed = false;
  bool success = false;
  std::string error;
  services.publishActivationEnvironment(
      environment, [&](bool ok, const std::string &message) {
        success = ok;
        error = message;
        completed = true;
      });
  require(waitFor(eventLoop, completed),
          "Activation environment helper did not finish.");
  require(success, error.c_str());
  const auto calls = readFile(logPath);
  const auto firstSystemd = calls.find("--systemd\n");
  require(firstSystemd != std::string::npos,
          "Activation helper was not first called with --systemd.");
  require(calls.find("--systemd\n", firstSystemd + 1) == std::string::npos,
          "Activation helper fallback repeated the --systemd option.");
  require(calls.find("WAYLAND_DISPLAY\n") != std::string::npos,
          "Activation helper did not receive the Wayland variable name.");
  ::unsetenv("LUDASH_FAKE_ACTIVATION_FAIL_SYSTEMD");
}

void portalFallbackRestartsServicesWithClientEnvironment(
    SessionServices &services, wl_event_loop *eventLoop,
    const ProcessEnvironment &environment,
    const std::filesystem::path &logPath) {
  std::filesystem::remove(logPath);
  bool completed = false;
  services.refreshScreencastPortalServices(environment,
                                           [&] { completed = true; });
  require(waitFor(eventLoop, completed), "Portal service restart timed out.");
  const auto invocation = readFile(logPath);
  require(invocation.find("--user\nrestart\n") == 0,
          "Portal fallback did not issue a user service restart.");
  require(invocation.find("xdg-desktop-portal-wlr.service\n") !=
              std::string::npos &&
              invocation.find("xdg-desktop-portal-lunadash.service\n") !=
                  std::string::npos &&
              invocation.find("xdg-desktop-portal-gtk.service\n") !=
                  std::string::npos &&
              invocation.find("xdg-desktop-portal.service\n") !=
                  std::string::npos,
          "Portal fallback omitted a service from the restart request.");
  require(invocation.find("WAYLAND_DISPLAY=wayland-session-test") !=
              std::string::npos,
          "Systemctl did not receive the compositor client environment.");
}

} // namespace
} // namespace LunaDash

int main() {
  try {
    LunaDash::TemporaryTree tree;
    const auto bin = tree.path() / "bin";
    const auto logPath = tree.path() / "calls.txt";
    LunaDash::writeExecutable(
        bin / "dbus-update-activation-environment",
        "#!/bin/sh\nprintf '%s\\n' \"$@\" >> \"$LUDASH_SESSION_TEST_LOG\"\n"
        "if [ \"$1\" = \"--systemd\" ] && [ \"$LUDASH_FAKE_ACTIVATION_FAIL_SYSTEMD\" = \"1\" ]; then\n"
        "  echo 'fake systemd publication failure' >&2\n  exit 1\nfi\n"
        "exit 0\n");
    LunaDash::writeExecutable(
        bin / "systemctl",
        "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$LUDASH_SESSION_TEST_LOG\"\n"
        "printf 'WAYLAND_DISPLAY=%s\\n' \"$WAYLAND_DISPLAY\" >> \"$LUDASH_SESSION_TEST_LOG\"\n"
        "exit 0\n");
    ::setenv("PATH", bin.c_str(), 1);
    ::setenv("LUDASH_SESSION_TEST_LOG", logPath.c_str(), 1);
    // Never start the host's xdg-desktop-portal-wlr, which would replace a running portal.
    ::setenv("LUDASH_XDPW", "", 1);

    wl_event_loop *eventLoop = wl_event_loop_create();
    if (!eventLoop)
      throw std::runtime_error("Could not create Wayland event loop.");
    {
      LunaDash::SessionServices services(eventLoop);
      const LunaDash::ProcessEnvironment environment{
          {"PATH", bin.string()},
          {"LUDASH_SESSION_TEST_LOG", logPath.string()},
          {"WAYLAND_DISPLAY", "wayland-session-test"},
          {"XDG_CURRENT_DESKTOP", "LunaDash"}};
      LunaDash::activationSystemdFailureFallsBackToBusEnvironment(
          services, eventLoop, environment, logPath);
      LunaDash::portalFallbackRestartsServicesWithClientEnvironment(
          services, eventLoop, environment, logPath);
    }
    wl_event_loop_destroy(eventLoop);
  } catch (const std::exception &error) {
    std::cerr << "Session services test failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
