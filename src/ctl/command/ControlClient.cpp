#include "ctl/command/ControlClient.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {
using Json = nlohmann::json;

std::filesystem::path runtimeDirectory() {
  if (const char *runtime = std::getenv("XDG_RUNTIME_DIR");
      runtime && *runtime)
    return runtime;
  return std::filesystem::path("/run/user") / std::to_string(getuid());
}

// The compositor publishes LUNADASH_CONTROL to everything it starts, but a
// terminal, editor or shell that was started outside that tree does not inherit
// it. Fall back to the session's standard control sockets, then to the single
// control socket in the runtime directory, so lunadashctl works from anywhere.
std::string defaultControlPath() {
  std::error_code error;
  const auto runtime = runtimeDirectory();
  for (const auto *name : {"lunadash-0-control", "ludash-0-control"}) {
    const auto candidate = runtime / name;
    if (std::filesystem::exists(candidate, error))
      return candidate.string();
    error.clear();
  }

  // A nested session or a custom --socket exposes one differently named
  // control socket. Several of them are ambiguous, so an explicit variable is
  // required.
  std::string found;
  std::filesystem::directory_iterator entry(runtime, error), end;
  for (; !error && entry != end; entry.increment(error)) {
    const auto filename = entry->path().filename().string();
    if (!filename.ends_with("-control"))
      continue;
    if (!found.empty())
      return {};
    found = entry->path().string();
  }
  return found;
}

bool waitFor(int fd, short events,
             std::chrono::steady_clock::time_point deadline) {
  while (true) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0)
      return false;
    pollfd descriptor{fd, events, 0};
    const auto timeout = static_cast<int>(
        std::min<std::int64_t>(remaining.count(), 1500));
    const int result = ::poll(&descriptor, 1, timeout);
    if (result > 0)
      return (descriptor.revents & events) != 0;
    if (result == 0)
      return false;
    if (errno != EINTR)
      return false;
  }
}

bool writeAll(int fd, std::string_view data,
              std::chrono::steady_clock::time_point deadline) {
  size_t offset = 0;
  while (offset < data.size()) {
    const auto count = ::send(fd, data.data() + offset, data.size() - offset,
                              MSG_NOSIGNAL | MSG_DONTWAIT);
    if (count > 0) {
      offset += static_cast<size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) &&
        waitFor(fd, POLLOUT, deadline))
      continue;
    return false;
  }
  return true;
}

bool readResponse(int fd, std::string &result,
                  std::chrono::steady_clock::time_point deadline) {
  std::array<char, 4096> buffer{};
  while (result.find('\n') == std::string::npos) {
    if (!waitFor(fd, POLLIN, deadline))
      return false;
    const auto count = ::recv(fd, buffer.data(), buffer.size(), 0);
    if (count <= 0)
      return false;
    result.append(buffer.data(), static_cast<size_t>(count));
    if (result.size() > 1024 * 1024)
      return false;
  }
  return true;
}

int connectControlSocket(const std::string &path,
                         std::chrono::steady_clock::time_point deadline) {
  sockaddr_un address{};
  if (path.size() >= sizeof(address.sun_path))
    return -1;
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -1;
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    ::close(fd);
    return -1;
  }

  address.sun_family = AF_UNIX;
  std::copy(path.begin(), path.end(), address.sun_path);
  if (::connect(fd, reinterpret_cast<const sockaddr *>(&address),
                sizeof(address)) < 0) {
    if (errno != EINPROGRESS || !waitFor(fd, POLLOUT, deadline)) {
      ::close(fd);
      return -1;
    }
    int error = 0;
    socklen_t length = sizeof(error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error) {
      ::close(fd);
      return -1;
    }
  }
  return fd;
}
} // namespace

int ControlClient::run(int argc, char **argv) {
  if (argc < 2) {
    std::cerr
        << "Usage: lunadashctl "
           "status|workspace|focus|group-window|expel-window|minimize|close|"
           "language|shortcut-capture|shortcuts|reset-shortcuts|check-update|"
           "capture|screenshot|brightness|ddc-brightness|ddc-refresh|display-configure|display-confirm|"
           "display-revert|"
           "wallpaper|choose-wallpaper|wallpaper-image|wallpaper-default|"
           "appearance|setup|finish-setup|configure-network|launch-x11|"
           "launch-command|launch-with-x11|"
           "open-settings|system-tool|audio|network|power-profile|desktop-size|"
           "reset-preferences|default-apps|launch-default|module-validate|"
           "module-save|module-reset|"
           "extension-save|extension-error|settings-describe|settings-update|"
           "window-layout-settings|window-layout-action|quit [value]\n"
           "  group-window value: {\"window\":ID,\"target\":ID}\n"
           "  launch-with-x11 -- program [argument ...]\n";
    return 2;
  }

  const std::string method(argv[1]);
  std::string value = argc > 2 ? argv[2] : "";
  if (method == "launch-with-x11" && value == "--") {
    if (argc < 4 || argv[3][0] == '\0') {
      std::cerr << "Expected a program after --.\n";
      return 2;
    }
    auto command = Json::array();
    for (int index = 3; index < argc; ++index)
      command.push_back(argv[index]);
    value = command.dump();
  }

  std::string path;
  if (const char *control = std::getenv("LUNADASH_CONTROL");
      control && *control)
    path = control;
  if (path.empty()) {
    if (const char *control = std::getenv("LUDASH_CONTROL");
        control && *control)
      path = control;
  }
  if (path.empty())
    path = defaultControlPath();
  if (path.empty()) {
    std::cerr
        << "LUNADASH_CONTROL is not set and no single LunaDash control socket "
           "was found in the runtime directory.\n";
    return 2;
  }

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(1500);
  const int socket = connectControlSocket(path, deadline);
  if (socket < 0)
    return 1;

  const auto request = Json{{"method", method}, {"value", value}}.dump() + "\n";
  if (!writeAll(socket, request, deadline)) {
    ::close(socket);
    return 1;
  }

  std::string result;
  const bool received = readResponse(socket, result, deadline);
  ::close(socket);
  if (!received)
    return 1;

  const auto newline = result.find('\n');
  const auto response = Json::parse(result.substr(0, newline), nullptr, false);
  std::cout.write(result.data(), static_cast<std::streamsize>(result.size()));
  return response.is_discarded() || !response.is_object() ||
                 response.contains("error")
             ? 1
             : 0;
}

} // namespace LunaDash
