#include "compositor/xwayland/XWaylandSupport.hpp"

#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <spawn.h>
#include <string_view>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace LunaDash {
namespace {
std::string findExecutable(std::string_view program) {
  if (program.empty())
    return {};
  const std::filesystem::path requested(program);
  if (requested.has_parent_path())
    return ::access(requested.c_str(), X_OK) == 0 ? requested.string()
                                                   : std::string{};

  const auto foundPath = std::getenv("PATH");
  if (!foundPath)
    return {};
  const std::string_view searchPath(foundPath);
  std::size_t start = 0;
  while (start <= searchPath.size()) {
    const auto separator = searchPath.find(':', start);
    const auto directory = searchPath.substr(
        start, separator == std::string_view::npos ? searchPath.size() - start
                                                   : separator - start);
    if (!directory.empty()) {
      const auto candidate = std::filesystem::path(directory) / requested;
      if (::access(candidate.c_str(), X_OK) == 0)
        return candidate.string();
    }
    if (separator == std::string_view::npos)
      break;
    start = separator + 1;
  }
  return {};
}

void setError(std::string *error, std::string value) {
  if (error)
    *error = std::move(value);
}
} // namespace

XWaylandSupport::~XWaylandSupport() { stop(); }

bool XWaylandSupport::start(wl_display *display, wlr_compositor *compositor,
                            wlr_seat *seat,
                            std::map<std::string, std::string> environment,
                            NewSurfaceCallback newSurface) {
  stop();
  environment_ = std::move(environment);
  newSurface_ = std::move(newSurface);
  seat_ = seat;
  eventLoop_ = display ? wl_display_get_event_loop(display) : nullptr;
  error_.clear();
  xwmReady_ = false;

#if !LUDASH_WLR_HAS_XWAYLAND
  (void)display;
  (void)compositor;
  (void)seat;
  error_ = "This wlroots build does not provide XWayland support.";
  return false;
#else
  if (!display || !compositor || !seat || !eventLoop_) {
    error_ = "The compositor seat is unavailable for XWayland.";
    return false;
  }
  if (findExecutable("Xwayland").empty()) {
    error_ = "Install Xwayland for X11 applications.";
    return false;
  }

  // wlroots owns the X server sockets and XWM. Lazy mode reserves DISPLAY now
  // and starts the Xwayland process only when an X11 client connects.
  xwayland_ = wlr_xwayland_create(display, compositor, true);
  if (!xwayland_) {
    error_ = "wlroots could not create the XWayland/XWM bridge.";
    return false;
  }

  xwayland_->data = this;
  wlr_xwayland_set_seat(xwayland_, seat_);
  listen<&XWaylandSupport::onReady>(&xwayland_->events.ready, ready_);
  listen<&XWaylandSupport::onNewSurface>(&xwayland_->events.new_surface,
                                         newSurfaceListener_);
  return true;
#endif
}

void XWaylandSupport::onReady() {
  if (!xwayland_)
    return;
#if LUDASH_WLR_HAS_XWAYLAND
  // XWM creates the X11 CLIPBOARD/PRIMARY/DnD bridge on ready. Point it at the
  // same seat used by wl_data_device and zwlr_data_control so X11 and Wayland
  // selections become one compositor-owned clipboard domain.
  wlr_xwayland_set_seat(xwayland_, seat_);
#endif
  xwmReady_ = true;
}

void XWaylandSupport::onNewSurface(wlr_xwayland_surface *surface) {
  if (surface && newSurface_)
    newSurface_(surface);
}

void XWaylandSupport::stop() {
  if (clientReaper_) {
    wl_event_source_remove(clientReaper_);
    clientReaper_ = nullptr;
  }

  std::vector<pid_t> remaining;
  for (const pid_t client : clients_) {
    int status = 0;
    pid_t result = 0;
    do {
      result = ::waitpid(client, &status, WNOHANG);
    } while (result < 0 && errno == EINTR);
    if (result == 0) {
      ::kill(client, SIGTERM);
      remaining.push_back(client);
    }
  }

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(800);
  while (!remaining.empty() && std::chrono::steady_clock::now() < deadline) {
    std::erase_if(remaining, [](pid_t client) {
      int status = 0;
      pid_t result = 0;
      do {
        result = ::waitpid(client, &status, WNOHANG);
      } while (result < 0 && errno == EINTR);
      return result == client || (result < 0 && errno == ECHILD);
    });
    if (!remaining.empty())
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (const pid_t client : remaining) {
    ::kill(client, SIGKILL);
    while (::waitpid(client, nullptr, 0) < 0 && errno == EINTR) {
    }
  }
  clients_.clear();

  Templates::detachListeners(newSurfaceListener_, ready_);
#if LUDASH_WLR_HAS_XWAYLAND
  if (xwayland_) {
    xwayland_->data = nullptr;
    wlr_xwayland_destroy(xwayland_);
  }
#endif
  xwayland_ = nullptr;
  seat_ = nullptr;
  eventLoop_ = nullptr;
  xwmReady_ = false;
  newSurface_ = {};
}

bool XWaylandSupport::stopped() const { return xwayland_ == nullptr; }

bool XWaylandSupport::startServer(std::string *error) {
  if (!xwayland_) {
    setError(error, error_.empty() ? "XWayland is unavailable." : error_);
    return false;
  }
  // Lazy wlr_xwayland is intentionally not spawned here. Exporting DISPLAY is
  // enough: the first X11 connection starts the server and then XWM.
  return true;
}

XWaylandSnapshot XWaylandSupport::snapshot() const {
  XWaylandSnapshot result;
#if LUDASH_WLR_HAS_XWAYLAND
  if (xwayland_) {
    result.display = xwayland_->display_name ? xwayland_->display_name : "";
    result.running = xwmReady_ ||
                     (xwayland_->server && xwayland_->server->pid > 0);
  }
#endif
  result.available = xwayland_ != nullptr && error_.empty();
  result.selectionBridge = xwmReady_;
  result.error = error_;
  return result;
}

void XWaylandSupport::applyEnvironment(
    std::map<std::string, std::string> &environment) const {
#if LUDASH_WLR_HAS_XWAYLAND
  if (xwayland_ && xwayland_->display_name && error_.empty()) {
    environment["DISPLAY"] = xwayland_->display_name;
    environment.erase("XAUTHORITY");
    return;
  }
#endif
  environment.erase("DISPLAY");
  environment.erase("XAUTHORITY");
}

bool XWaylandSupport::ensureClientReaper() {
  if (clientReaper_)
    return true;
  if (!eventLoop_)
    return false;
  clientReaper_ = timer<&XWaylandSupport::reapClients>(eventLoop_);
  return clientReaper_ != nullptr;
}

void XWaylandSupport::reapClients() {
  std::erase_if(clients_, [](pid_t client) {
    int status = 0;
    pid_t result = 0;
    do {
      result = ::waitpid(client, &status, WNOHANG);
    } while (result < 0 && errno == EINTR);
    return result == client || (result < 0 && errno == ECHILD);
  });
  if (clientReaper_)
    wl_event_source_timer_update(clientReaper_, clients_.empty() ? 0 : 250);
}

bool XWaylandSupport::launch(const std::vector<std::string> &command,
                             std::string *error) {
  if (command.empty() || command.size() > 64 || command.front().empty()) {
    setError(error, "Enter an executable and its arguments.");
    return false;
  }
  const std::string executable = findExecutable(command.front());
  if (executable.empty()) {
    setError(error, "Executable not found: " + command.front());
    return false;
  }
  if (!startServer(error))
    return false;
  if (!ensureClientReaper()) {
    setError(error, "Could not monitor X11 application processes.");
    return false;
  }

  auto environment = environment_;
  applyEnvironment(environment);
  environment.erase("WAYLAND_DISPLAY");
  environment["QT_QPA_PLATFORM"] = "xcb";
  environment["GDK_BACKEND"] = "x11";
  environment["SDL_VIDEODRIVER"] = "x11";
  environment["_JAVA_AWT_WM_NONREPARENTING"] = "1";
  environment.erase("LIBGL_ALWAYS_SOFTWARE");

  std::vector<char *> arguments;
  arguments.reserve(command.size() + 1);
  arguments.push_back(const_cast<char *>(executable.c_str()));
  for (std::size_t index = 1; index < command.size(); ++index)
    arguments.push_back(const_cast<char *>(command[index].c_str()));
  arguments.push_back(nullptr);

  std::vector<std::string> environmentEntries;
  environmentEntries.reserve(environment.size());
  for (const auto &[name, value] : environment)
    environmentEntries.push_back(name + "=" + value);
  std::vector<char *> environmentPointers;
  environmentPointers.reserve(environmentEntries.size() + 1);
  for (auto &entry : environmentEntries)
    environmentPointers.push_back(entry.data());
  environmentPointers.push_back(nullptr);

  pid_t process = 0;
  const int status = ::posix_spawn(
      &process, executable.c_str(), nullptr, nullptr, arguments.data(),
      environmentPointers.data());
  if (status != 0) {
    setError(error, "Could not launch X11 application: " +
                         std::string(std::strerror(status)));
    return false;
  }
  clients_.push_back(process);
  wl_event_source_timer_update(clientReaper_, 250);
  return true;
}

void applyXWaylandEnvironment(std::map<std::string, std::string> &environment,
                              const XWaylandSupport *support) {
  std::map<std::string, std::string> displayEnvironment;
  for (const auto *name : {"DISPLAY", "XAUTHORITY"}) {
    if (const auto found = environment.find(name); found != environment.end())
      displayEnvironment.emplace(name, found->second);
  }
  if (support)
    support->applyEnvironment(displayEnvironment);
  else {
    displayEnvironment.erase("DISPLAY");
    displayEnvironment.erase("XAUTHORITY");
  }
  for (const auto *name : {"DISPLAY", "XAUTHORITY"}) {
    environment.erase(name);
    if (const auto found = displayEnvironment.find(name);
        found != displayEnvironment.end())
      environment.emplace(name, found->second);
  }
}

} // namespace LunaDash
