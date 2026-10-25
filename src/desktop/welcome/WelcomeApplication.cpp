#include "desktop/welcome/WelcomeApplication.hpp"

#include "Welcome.h"

#include <nlohmann/json.hpp>
#include <slint.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace LunaDash {
namespace {
using Json = nlohmann::json;
using Deadline = std::chrono::steady_clock::time_point;

std::filesystem::path runtimeDirectory() {
  if (const char *runtime = std::getenv("XDG_RUNTIME_DIR");
      runtime && *runtime)
    return runtime;
  return std::filesystem::path("/run/user") / std::to_string(getuid());
}

std::string controlPath() {
  for (const auto *name : {"LUNADASH_CONTROL", "LUDASH_CONTROL"})
    if (const char *value = std::getenv(name); value && *value)
      return value;

  std::error_code error;
  const auto runtime = runtimeDirectory();
  for (const auto *name : {"lunadash-0-control", "ludash-0-control"}) {
    const auto candidate = runtime / name;
    if (std::filesystem::exists(candidate, error))
      return candidate.string();
    error.clear();
  }
  std::string found;
  std::filesystem::directory_iterator entry(runtime, error), end;
  for (; !error && entry != end; entry.increment(error)) {
    if (!entry->path().filename().string().ends_with("-control"))
      continue;
    if (!found.empty())
      return {};
    found = entry->path().string();
  }
  return found;
}

bool waitFor(int fd, short events, Deadline deadline) {
  while (true) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0)
      return false;
    pollfd descriptor{fd, events, 0};
    const int result = ::poll(&descriptor, 1, static_cast<int>(
        std::min<std::int64_t>(remaining.count(), 1500)));
    if (result > 0)
      return (descriptor.revents & events) != 0;
    if (result == 0)
      return false;
    if (errno != EINTR)
      return false;
  }
}

int connectControl(const std::string &path, Deadline deadline) {
  sockaddr_un address{};
  if (path.empty() || path.size() >= sizeof(address.sun_path))
    return -1;
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -1;
  address.sun_family = AF_UNIX;
  std::copy(path.begin(), path.end(), address.sun_path);
  if (::connect(fd, reinterpret_cast<const sockaddr *>(&address),
                sizeof(address)) < 0) {
    const int connectError = errno;
    ::close(fd);
    errno = connectError;
    return -1;
  }
  (void)deadline;
  return fd;
}

bool writeAll(int fd, std::string_view data, Deadline deadline) {
  size_t offset = 0;
  while (offset < data.size()) {
    const auto count = ::send(fd, data.data() + offset, data.size() - offset,
                              MSG_NOSIGNAL);
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

Json request(std::string_view method, std::string_view value = {}) {
  constexpr auto timeout = std::chrono::milliseconds(1500);
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  const int fd = connectControl(controlPath(), deadline);
  if (fd < 0)
    return Json{{"error", "Could not contact the desktop."}};

  const auto line = Json{{"method", method}, {"value", value}}.dump() + "\n";
  if (!writeAll(fd, line, deadline)) {
    ::close(fd);
    return Json{{"error", "Could not contact the desktop."}};
  }

  std::string response;
  std::array<char, 8192> buffer{};
  while (response.find('\n') == std::string::npos && response.size() <= 1024 * 1024) {
    if (!waitFor(fd, POLLIN, deadline))
      break;
    const auto count = ::recv(fd, buffer.data(), buffer.size(), 0);
    if (count <= 0)
      break;
    response.append(buffer.data(), static_cast<size_t>(count));
  }
  ::close(fd);
  const auto newline = response.find('\n');
  if (newline == std::string::npos)
    return Json{{"error", "Could not contact the desktop."}};
  auto result = Json::parse(response.substr(0, newline), nullptr, false);
  if (result.is_discarded() || !result.is_object())
    return Json{{"error", "Could not contact the desktop."}};
  return result;
}

std::string text(const Json &object, std::string_view key,
                 std::string fallback = {}) {
  const auto value = object.find(std::string(key));
  return value != object.end() && value->is_string() ? value->get<std::string>()
                                                    : std::move(fallback);
}

std::string translated(const Json &translations, std::string_view source) {
  return text(translations, source, std::string(source));
}

slint::Color color(const Json &palette, std::string_view key,
                   std::uint32_t fallback) {
  const auto value = text(palette, key);
  if (value.size() == 7 && value.front() == '#') {
    try {
      return slint::Color::from_argb_encoded(
          0xff000000u | static_cast<std::uint32_t>(std::stoul(value.substr(1), nullptr, 16)));
    } catch (const std::exception &) {
    }
  }
  return slint::Color::from_argb_encoded(fallback);
}

void applyState(slint::ComponentHandle<WelcomeWindow> &window,
                const Json &state) {
  const auto translations = state.value("translations", Json::object());
  const auto appearance = state.value("appearance", Json::object());
  const auto palette = state.value("palette", Json::object());
  const auto shortcuts = state.value("shortcuts", Json::object());
  const auto tr = [&translations](std::string_view key) {
    return translated(translations, key);
  };
  const auto set = [&window](auto setter, const std::string &value) {
    (window.operator->()->*setter)(slint::SharedString(value));
  };

  const auto locale = text(state, "language", "en_US");
  set(&WelcomeWindow::set_selected_locale, locale);
  set(&WelcomeWindow::set_window_title, "LunaDash · " + tr("Welcome"));
  set(&WelcomeWindow::set_welcome_title, tr("Welcome to LunaDash"));
  set(&WelcomeWindow::set_introduction, tr("Make yourself at home. Choose a comfortable appearance and learn your way around."));
  set(&WelcomeWindow::set_language_heading, tr("Interface language"));
  set(&WelcomeWindow::set_motion_heading, tr("Reduced motion"));
  set(&WelcomeWindow::set_motion_value,
      tr(appearance.value("animations", true) ? "Off" : "On"));
  set(&WelcomeWindow::set_setup_heading, tr("Make this space yours"));
  set(&WelcomeWindow::set_steps_heading, tr("Your first steps"));
  set(&WelcomeWindow::set_settings_note,
      tr("Settings stay available from the panel whenever you need them."));
  set(&WelcomeWindow::set_shortcuts_heading, tr("Find your rhythm"));
  set(&WelcomeWindow::set_files_label, tr("Files"));
  set(&WelcomeWindow::set_terminal_label, tr("Your configured terminal"));
  set(&WelcomeWindow::set_control_center_label, tr("Control center"));
  set(&WelcomeWindow::set_development_note,
      tr("Development preview. Some session and protocol features are still being improved."));
  set(&WelcomeWindow::set_documentation_label, tr("Documentation"));
  set(&WelcomeWindow::set_start_label, tr("Start desktop"));
  set(&WelcomeWindow::set_close_label, tr("Close"));
  set(&WelcomeWindow::set_error_message,
      text(state, "error").empty()
          ? text(state, "welcomeError")
          : tr("Could not contact the desktop."));
  set(&WelcomeWindow::set_files_shortcut,
      text(shortcuts, "launchFiles", tr("Disabled")));
  set(&WelcomeWindow::set_terminal_shortcut,
      text(shortcuts, "launchTerminal", tr("Disabled")));
  set(&WelcomeWindow::set_control_center_shortcut,
      text(shortcuts, "openControlCenter", tr("Disabled")));

  const bool animations = appearance.value("animations", true);
  window->set_reduced_motion(!animations);
  window->set_dark_theme(palette.value("dark", true));
  window->set_background_color(color(palette, "background", 0xff171922));
  window->set_surface_color(color(palette, "surface", 0xff20232e));
  window->set_elevated_color(color(palette, "surfaceElevated", 0xff292d39));
  window->set_surface_hover_color(color(palette, "surfaceHover", 0xff353b4b));
  window->set_text_color(color(palette, "text", 0xfff1f1f5));
  window->set_muted_color(color(palette, "muted", 0xffa5a8b5));
  window->set_border_color(color(palette, "border", 0xff4c5060));
  window->set_accent_color(color(palette, "accent", 0xff9ccbfb));
  window->set_accent_ink_color(color(palette, "accentInk", 0xff171922));
  set(&WelcomeWindow::set_appearance_label, tr("Appearance"));
  set(&WelcomeWindow::set_applications_label, tr("Default applications"));
  set(&WelcomeWindow::set_plugins_label, tr("Plugins"));
  set(&WelcomeWindow::set_keyboard_label, tr("Keyboard shortcuts"));
}
} // namespace

int WelcomeApplication::run(int argc, char **argv) {
  (void)argc;
  (void)argv;
  auto window = WelcomeWindow::create();
  auto state = request("status");
  applyState(window, state);

  window->on_language_selected([&](const slint::SharedString &locale) {
    state = request("language", std::string(locale.data(), locale.size()));
    applyState(window, state);
  });
  window->on_reduced_motion_toggled([&] {
    const bool currentlyAnimated =
        state.value("appearance", Json::object()).value("animations", true);
    state = request("appearance", Json{{"animations", !currentlyAnimated}}.dump());
    applyState(window, state);
  });
  window->on_settings_requested([&](const slint::SharedString &page) {
    state = request("open-settings", page.data());
    if (state.contains("error"))
      applyState(window, state);
  });
  window->on_documentation_requested([&] {
    state = request("open-url", "https://luyishan-4.github.io/LunaDash/");
    if (state.contains("error"))
      applyState(window, state);
  });
  // The window is frameless; its header has the close button.
  window->on_close_requested([&] {
    window->hide();
    slint::quit_event_loop();
  });
  window->on_finish_requested([&] {
    state = request("finish-setup");
    if (state.contains("error"))
      applyState(window, state);
    else {
      window->hide();
      slint::quit_event_loop();
    }
  });

  try {
    window->run();
  } catch (const std::exception &error) {
    std::cerr << "LunaDash Welcome could not start: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
} // namespace LunaDash
