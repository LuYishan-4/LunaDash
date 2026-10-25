#include "desktop/package/PackageManagerApplication.hpp"

#include "PackageManager.h"
#include "config/appearance/AppearancePalette.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/localization/Localization.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/package/PackageManager.hpp"

#include <gio/gio.h>
#include <slint.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <poll.h>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

extern char **environ;

namespace LunaDash {
namespace {

using Json = nlohmann::json;
constexpr std::size_t maximumOutputBytes = 1024 * 1024;

std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
    return std::isspace(c);
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
    return std::isspace(c);
  }).base();
  if (first >= last)
    return {};
  return {first, last};
}

std::filesystem::path configDirectory() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg &&
      std::filesystem::path(xdg).is_absolute())
    return xdg;
  if (const char *home = std::getenv("HOME"); home && *home)
    return std::filesystem::path(home) / ".config";
  return {};
}

std::string keyFileString(GKeyFile *file, const char *group, const char *key) {
  GError *error = nullptr;
  char *value = g_key_file_get_string(file, group, key, &error);
  if (error) {
    g_error_free(error);
    return {};
  }
  std::string result = value ? value : "";
  g_free(value);
  return result;
}

GKeyFile *loadLegacySettings() {
  auto *file = g_key_file_new();
  const auto root = configDirectory();
  if (root.empty())
    return file;
  const auto path = root / "LunaDash/LunaDash.conf";
  GError *error = nullptr;
  if (!g_key_file_load_from_file(file, path.c_str(), G_KEY_FILE_NONE, &error)) {
    if (error)
      g_error_free(error);
  }
  return file;
}

std::string translated(const Json &catalog, std::string_view source) {
  const auto found = catalog.find(std::string(source));
  return found != catalog.end() && found->is_string()
             ? found->get<std::string>()
             : std::string(source);
}

slint::Color paletteColor(const Json &palette, std::string_view key,
                          std::uint32_t fallback) {
  const auto found = palette.find(std::string(key));
  if (found != palette.end() && found->is_string()) {
    const auto value = found->get<std::string>();
    if (value.size() == 7 && value.front() == '#') {
      try {
        return slint::Color::from_argb_encoded(
            0xff000000u | static_cast<std::uint32_t>(
                              std::stoul(value.substr(1), nullptr, 16)));
      } catch (const std::exception &) {
      }
    }
  }
  return slint::Color::from_argb_encoded(fallback);
}

std::string findExecutable(std::string_view executable) {
  if (executable.empty())
    return {};
  const std::filesystem::path requested(executable);
  if (requested.has_parent_path())
    return ::access(requested.c_str(), X_OK) == 0 ? requested.string() : "";
  const char *pathValue = std::getenv("PATH");
  if (!pathValue)
    return {};
  std::string searchPath(pathValue);
  std::size_t begin = 0;
  while (begin <= searchPath.size()) {
    const auto end = searchPath.find(':', begin);
    const auto part = searchPath.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin);
    const auto directory = part.empty() ? std::filesystem::current_path()
                                        : std::filesystem::path(part);
    const auto candidate = directory / requested;
    if (::access(candidate.c_str(), X_OK) == 0)
      return candidate.string();
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return {};
}

std::string sharedString(const slint::SharedString &value) {
  return {value.data(), value.size()};
}

std::string displayCommand(const std::vector<std::string> &command) {
  std::string result;
  for (const auto &argument : command) {
    if (!result.empty())
      result.push_back(' ');
    if (argument.find_first_of(" \t\n'\"") == std::string::npos) {
      result += argument;
      continue;
    }
    result.push_back('\'');
    for (const char character : argument) {
      if (character == '\'')
        result += "'\\''";
      else
        result.push_back(character);
    }
    result.push_back('\'');
  }
  return result;
}

void trimOutput(std::string &output) {
  if (output.size() <= maximumOutputBytes)
    return;
  const auto keepFrom = output.size() - maximumOutputBytes;
  const auto newline = output.find('\n', keepFrom);
  output.erase(0, newline == std::string::npos ? keepFrom : newline + 1);
  output.insert(0, "[Earlier output trimmed]\n");
}

bool spawnPacman(const std::string &pacman,
                 const std::vector<std::string> &arguments,
                 pid_t *pid, int *readDescriptor, std::string *error) {
  int descriptors[2] = {-1, -1};
  if (::pipe2(descriptors, O_CLOEXEC) < 0) {
    if (error)
      *error = std::strerror(errno);
    return false;
  }
  posix_spawn_file_actions_t actions;
  int status = ::posix_spawn_file_actions_init(&actions);
  const bool actionsInitialized = status == 0;
  if (status == 0)
    status = ::posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO);
  if (status == 0)
    status = ::posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDERR_FILENO);
  if (status == 0)
    status = ::posix_spawn_file_actions_addclose(&actions, descriptors[0]);
  if (status == 0)
    status = ::posix_spawn_file_actions_addclose(&actions, descriptors[1]);

  std::vector<std::string> storage;
  storage.reserve(arguments.size() + 1);
  storage.push_back(pacman);
  storage.insert(storage.end(), arguments.begin(), arguments.end());
  std::vector<char *> argv;
  argv.reserve(storage.size() + 1);
  for (auto &value : storage)
    argv.push_back(value.data());
  argv.push_back(nullptr);

  posix_spawnattr_t attributes;
  bool attributesInitialized = false;
  if (status == 0) {
    status = ::posix_spawnattr_init(&attributes);
    attributesInitialized = status == 0;
  }
  if (status == 0)
    status = ::posix_spawnattr_setpgroup(&attributes, 0);
  if (status == 0)
    status = ::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
  if (status == 0)
    status = ::posix_spawnp(pid, pacman.c_str(), &actions, &attributes,
                            argv.data(), environ);
  if (attributesInitialized)
    ::posix_spawnattr_destroy(&attributes);
  if (actionsInitialized)
    ::posix_spawn_file_actions_destroy(&actions);
  ::close(descriptors[1]);
  if (status != 0) {
    ::close(descriptors[0]);
    if (error)
      *error = std::strerror(status);
    return false;
  }
  const int currentFlags = ::fcntl(descriptors[0], F_GETFL, 0);
  if (currentFlags < 0 ||
      ::fcntl(descriptors[0], F_SETFL, currentFlags | O_NONBLOCK) < 0) {
    if (error)
      *error = std::strerror(errno);
    ::close(descriptors[0]);
    ::kill(-*pid, SIGTERM);
    while (::waitpid(*pid, nullptr, 0) < 0 && errno == EINTR) {
    }
    return false;
  }
  *readDescriptor = descriptors[0];
  return true;
}

bool appendTerminalExecuteOption(std::vector<std::string> &command) {
  if (command.empty())
    return false;
  const auto terminal = std::filesystem::path(command.front()).filename();
  const auto name = terminal.string();
  if (name == "gnome-terminal" || name == "kgx" || name == "ptyxis") {
    command.push_back("--");
    return true;
  }
  if (name == "xfce4-terminal") {
    command.push_back("--execute");
    return true;
  }
  if (name == "konsole" || name == "foot" || name == "alacritty" ||
      name == "kitty" || name == "xterm") {
    command.push_back("-e");
    return true;
  }
  return false;
}

int runDetached(const std::vector<std::string> &command) {
  if (command.empty())
    return EINVAL;
  std::vector<std::string> storage = command;
  std::vector<char *> argv;
  argv.reserve(storage.size() + 1);
  for (auto &argument : storage)
    argv.push_back(argument.data());
  argv.push_back(nullptr);

  posix_spawnattr_t attributes;
  int result = ::posix_spawnattr_init(&attributes);
  if (result != 0)
    return result;
  pid_t pid = -1;
#if defined(POSIX_SPAWN_SETSID)
  result = ::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSID);
  if (result == 0)
#endif
  {
    result = ::posix_spawnp(&pid, storage.front().c_str(), nullptr, &attributes,
                            argv.data(), environ);
  }
  ::posix_spawnattr_destroy(&attributes);
  if (result == 0) {
    try {
      std::thread([pid] {
        int status = 0;
        while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
      }).detach();
    } catch (const std::system_error &error) {
      return error.code().value();
    }
  }
  return result;
}

} // namespace

int PackageManagerApplication::run(int argc, char **argv) {
  (void)argc;
  (void)argv;
  auto window = PackageManagerWindow::create();
  auto *legacySettings = loadLegacySettings();
  const auto locale = LunaDash::selectedLanguage();
  const auto catalog = languageDictionary(locale);
  auto text = [&catalog](std::string_view source) {
    return translated(catalog, source);
  };
  auto set = [&window](auto setter, const std::string &value) {
    (window.operator->()->*setter)(slint::SharedString(value));
  };

  set(&PackageManagerWindow::set_window_title,
      "LunaDash · " + text("Package manager"));
  set(&PackageManagerWindow::set_heading, text("Package manager"));
  set(&PackageManagerWindow::set_description,
      text("pacman: queries run without administrator privileges. Confirm installation and removal in a terminal."));
  set(&PackageManagerWindow::set_package_placeholder,
      text("Package name, for example fcitx5"));
  set(&PackageManagerWindow::set_installed_label, text("Installed"));
  set(&PackageManagerWindow::set_search_label, text("Search"));
  set(&PackageManagerWindow::set_install_label, text("Install"));
  set(&PackageManagerWindow::set_remove_label, text("Remove"));
  set(&PackageManagerWindow::set_upgrade_label, text("Full upgrade"));
  set(&PackageManagerWindow::set_output_label, text("Output"));
  set(&PackageManagerWindow::set_confirm_title, text("Confirm in terminal"));
  set(&PackageManagerWindow::set_confirm_copy,
      text("The terminal will run:"));
  set(&PackageManagerWindow::set_confirm_label, text("Open terminal"));
  set(&PackageManagerWindow::set_cancel_label, text("Cancel"));
  set(&PackageManagerWindow::set_close_label, text("Close"));

  Json preferences = loadDesktopPreferences();
  const auto legacyMode = keyFileString(legacySettings, "desktop", "themeMode");
  const auto legacyAccent = keyFileString(legacySettings, "desktop", "accent");
  if (!legacyMode.empty())
    preferences["themeMode"] = legacyMode;
  if (!legacyAccent.empty())
    preferences["accent"] = legacyAccent;
  const auto palette = appearancePalette(preferences);
  window->set_background_color(paletteColor(palette, "background", 0xff171922));
  window->set_surface_color(paletteColor(palette, "surface", 0xff20232e));
  window->set_elevated_color(
      paletteColor(palette, "surfaceElevated", 0xff292d39));
  window->set_text_color(paletteColor(palette, "text", 0xfff1f1f5));
  window->set_muted_color(paletteColor(palette, "muted", 0xffa5a8b5));
  window->set_border_color(paletteColor(palette, "border", 0xff4c5060));
  window->set_accent_color(paletteColor(palette, "accent", 0xff9ccbfb));
  window->set_accent_ink_color(
      paletteColor(palette, "accentInk", 0xff171922));

  const std::string pacman = findExecutable("pacman");
  window->set_pacman_available(!pacman.empty());
  if (pacman.empty())
    set(&PackageManagerWindow::set_output,
        text("pacman is not available on this system. Other desktop features remain available."));

  slint::ComponentWeakHandle<PackageManagerWindow> weakWindow(window);
  std::atomic<bool> busy{false};
  std::atomic<pid_t> activePid{-1};
  std::jthread queryThread;
  const auto postState = [weakWindow](std::string output,
                                      std::string status, bool isBusy) {
    slint::invoke_from_event_loop(
        [weakWindow, output = std::move(output), status = std::move(status),
         isBusy] {
          if (auto locked = weakWindow.lock()) {
            (*locked)->set_output(slint::SharedString(output));
            (*locked)->set_status(slint::SharedString(status));
            (*locked)->set_busy(isBusy);
          }
        });
  };

  const auto startQuery = [&](std::vector<std::string> arguments) {
    if (pacman.empty())
      return;
    if (busy.exchange(true)) {
      window->set_status(slint::SharedString(text("A package query is already running.")));
      return;
    }
    if (queryThread.joinable())
      queryThread.join();
    window->set_busy(true);
    window->set_status({});
    window->set_output({});
    queryThread = std::jthread(
        [&, arguments = std::move(arguments)](std::stop_token stop) {
          pid_t child = -1;
          int outputDescriptor = -1;
          std::string error;
          if (stop.stop_requested() ||
              !spawnPacman(pacman, arguments, &child, &outputDescriptor, &error)) {
            busy.store(false);
            postState(error.empty() ? std::string{} : error,
                      error.empty() ? std::string{} : text("Could not start pacman: ") + error,
                      false);
            return;
          }
          activePid.store(child);
          if (stop.stop_requested())
            ::kill(-child, SIGTERM);
          std::string output;
          std::array<char, 8192> buffer{};
          bool pipeOpen = true;
          bool childExited = false;
          bool signaled = false;
          auto signalTime = std::chrono::steady_clock::time_point{};
          auto childExitTime = std::chrono::steady_clock::time_point{};
          int waitStatus = 0;
          auto nextPublish = std::chrono::steady_clock::now();
          while (pipeOpen || !childExited) {
            if (stop.stop_requested() && !signaled) {
              ::kill(-child, SIGTERM);
              signalTime = std::chrono::steady_clock::now();
              signaled = true;
            } else if (signaled && !childExited &&
                       std::chrono::steady_clock::now() - signalTime >
                           std::chrono::milliseconds(800)) {
              ::kill(-child, SIGKILL);
            }
            pollfd descriptor{outputDescriptor, POLLIN | POLLHUP, 0};
            const int ready = ::poll(&descriptor, 1, 100);
            if (ready > 0 && (descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
              while (true) {
                const auto count = ::read(outputDescriptor, buffer.data(), buffer.size());
                if (count > 0) {
                  output.append(buffer.data(), static_cast<std::size_t>(count));
                  trimOutput(output);
                  continue;
                }
                if (count == 0) {
                  pipeOpen = false;
                  ::close(outputDescriptor);
                  outputDescriptor = -1;
                } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                  pipeOpen = false;
                  ::close(outputDescriptor);
                  outputDescriptor = -1;
                }
                break;
              }
            }
            if (!childExited) {
              const auto result = ::waitpid(child, &waitStatus, WNOHANG);
              if (result == child) {
                childExited = true;
                childExitTime = std::chrono::steady_clock::now();
              } else if (result < 0 && errno != EINTR) {
                childExited = true;
                childExitTime = std::chrono::steady_clock::now();
                waitStatus = 0;
              }
            }
            if (childExited && pipeOpen &&
                std::chrono::steady_clock::now() - childExitTime >
                    std::chrono::seconds(2)) {
              pipeOpen = false;
              ::close(outputDescriptor);
              outputDescriptor = -1;
            }
            if (std::chrono::steady_clock::now() >= nextPublish) {
              postState(output, {}, true);
              nextPublish = std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(100);
            }
          }
          if (outputDescriptor >= 0)
            ::close(outputDescriptor);
          if (!childExited) {
            while (::waitpid(child, &waitStatus, 0) < 0 && errno == EINTR) {
            }
          }
          activePid.store(-1);
          const int exitCode = WIFEXITED(waitStatus) ? WEXITSTATUS(waitStatus) : 128;
          output += "\n[Exit code " + std::to_string(exitCode) + "]";
          busy.store(false);
          postState(std::move(output), {}, false);
        });
  };

  const auto packageName = [&window] {
    return trim(sharedString(window->get_package_name()));
  };
  window->on_installed_requested([&] { startQuery({"-Q"}); });
  window->on_search_requested([&] {
    const auto package = packageName();
    if (package.empty()) {
      window->set_status(slint::SharedString(text("Enter a package name to search.")));
      return;
    }
    startQuery({"-Ss", "--", package});
  });

  std::vector<std::string> pendingTransaction;
  const auto requestTransaction = [&](std::vector<std::string> arguments) {
    if (arguments.empty()) {
      window->set_status(slint::SharedString(text("Enter one valid package name.")));
      return;
    }
    const auto sudo = findExecutable("sudo");
    if (sudo.empty()) {
      window->set_status(slint::SharedString(text("Ask your system administrator to manage packages.")));
      return;
    }
    std::string terminalError;
    auto terminal = defaultApplicationCommand("terminal", &terminalError);
    if (terminal.empty() || findExecutable(terminal.front()).empty()) {
      window->set_status(slint::SharedString(
          text("Install or configure a supported terminal to manage packages.")));
      return;
    }
    if (!appendTerminalExecuteOption(terminal)) {
      window->set_status(slint::SharedString(
          text("The configured terminal is not supported for package actions.")));
      return;
    }
    pendingTransaction = terminal;
    pendingTransaction.push_back(sudo);
    pendingTransaction.push_back(pacman);
    pendingTransaction.insert(pendingTransaction.end(), arguments.begin(), arguments.end());
    window->set_confirm_command(slint::SharedString(displayCommand(pendingTransaction)));
    window->set_confirmation_visible(true);
    window->set_status({});
  };
  window->on_install_requested([&] {
    requestTransaction(packageTransactionArguments("install", packageName()));
  });
  window->on_remove_requested([&] {
    requestTransaction(packageTransactionArguments("remove", packageName()));
  });
  window->on_upgrade_requested([&] {
    requestTransaction(packageTransactionArguments("upgrade"));
  });
  window->on_transaction_cancelled([&] {
    pendingTransaction.clear();
    window->set_confirmation_visible(false);
  });
  window->on_transaction_confirmed([&] {
    if (pendingTransaction.empty()) {
      window->set_confirmation_visible(false);
      return;
    }
    const auto result = runDetached(pendingTransaction);
    if (result != 0)
      window->set_status(slint::SharedString(text("Launch failed: ") +
                                             std::strerror(result)));
    else
      window->set_status(slint::SharedString(text("Package transaction opened in the configured terminal.")));
    pendingTransaction.clear();
    window->set_confirmation_visible(false);
  });

  // The window is frameless; its header has the close button.
  window->on_close_requested([&] {
    window->hide();
    slint::quit_event_loop();
  });

  g_key_file_unref(legacySettings);
  if (!pacman.empty())
    startQuery({"-Q"});

  int result = 0;
  try {
    window->run();
  } catch (const std::exception &error) {
    std::cerr << "Could not run the Package Manager window: " << error.what()
              << '\n';
    result = 1;
  }
  if (queryThread.joinable()) {
    queryThread.request_stop();
    const auto child = activePid.load();
    if (child > 0)
      ::kill(-child, SIGTERM);
    queryThread.join();
  }
  return result;
}

} // namespace LunaDash
