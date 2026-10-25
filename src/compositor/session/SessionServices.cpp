#include "compositor/session/SessionServices.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace LunaDash {
namespace {

constexpr const char *kActivationVariables[] = {"WAYLAND_DISPLAY",
                                                "DISPLAY",
                                                "XAUTHORITY",
                                                "XDG_SESSION_TYPE",
                                                "XDG_CURRENT_DESKTOP",
                                                "XDG_SESSION_DESKTOP",
                                                "GTK_USE_PORTAL",
                                                "QT_QPA_PLATFORMTHEME",
                                                "LUNADASH_CONTROL",
                                                "LUDASH_CONTROL",
                                                "QT_QPA_PLATFORM",
                                                "GDK_BACKEND",
                                                "SDL_VIDEODRIVER",
                                                "GLFW_PLATFORM",
                                                "LUNADASH_CHROMIUM_WAYLAND",
                                                "ELECTRON_OZONE_PLATFORM_HINT",
                                                "XMODIFIERS",
                                                "QT_IM_MODULE",
                                                "QT_IM_MODULES",
                                                "GTK_IM_MODULE",
                                                "SDL_IM_MODULE",
                                                "INPUT_METHOD",
                                                "http_proxy",
                                                "HTTP_PROXY",
                                                "https_proxy",
                                                "HTTPS_PROXY",
                                                "all_proxy",
                                                "ALL_PROXY",
                                                "no_proxy",
                                                "NO_PROXY"};

bool executableFile(const std::filesystem::path &path) {
  struct stat status{};
  return ::stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
         ::access(path.c_str(), X_OK) == 0;
}

std::string findExecutable(std::string_view name) {
  const std::filesystem::path requested(name);
  if (requested.has_parent_path())
    return executableFile(requested) ? requested.string() : std::string{};

  const char *pathValue = std::getenv("PATH");
  const std::string_view path =
      pathValue && *pathValue ? pathValue : "/usr/local/bin:/usr/bin:/bin";
  std::size_t start = 0;
  while (start <= path.size()) {
    const auto separator = path.find(':', start);
    const auto directory = path.substr(
        start, separator == std::string_view::npos ? path.size() - start
                                                   : separator - start);
    if (!directory.empty()) {
      const auto candidate = std::filesystem::path(directory) / requested;
      if (executableFile(candidate))
        return candidate.string();
    }
    if (separator == std::string_view::npos)
      break;
    start = separator + 1;
  }
  return {};
}

std::string findXdpw() {
  // LUDASH_XDPW names the executable; set but empty, no private instance is started (tests).
  if (const char *configured = std::getenv("LUDASH_XDPW"))
    return configured;
  auto executable = findExecutable("xdg-desktop-portal-wlr");
  if (!executable.empty())
    return executable;
  for (const auto *candidate : {"/usr/lib/xdg-desktop-portal-wlr",
                                "/usr/libexec/xdg-desktop-portal-wlr",
                                "/usr/local/libexec/xdg-desktop-portal-wlr"})
    if (executableFile(candidate))
      return candidate;
  return {};
}

std::string diagnostic(const std::string &output) {
  constexpr std::size_t kMaximumDiagnostic = 1024;
  const auto first = output.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = output.find_last_not_of(" \t\r\n");
  return output.substr(first, std::min(last - first + 1, kMaximumDiagnostic));
}

} // namespace

SessionServices::SessionServices(wl_event_loop *eventLoop)
    : activationProcess_(eventLoop), portalProcess_(eventLoop, false),
      systemctlProcess_(eventLoop) {}

void SessionServices::publishActivationEnvironment(
    const ProcessEnvironment &environment, ActivationCompletion completion) {
  const auto callback =
      std::make_shared<ActivationCompletion>(std::move(completion));
  const auto finish = [callback](bool success, const std::string &message) {
    if (!*callback)
      return;
    auto completion = std::move(*callback);
    *callback = {};
    completion(success, message);
  };
  const auto tool = findExecutable("dbus-update-activation-environment");
  if (tool.empty()) {
    finish(false, "dbus-update-activation-environment is not installed.");
    return;
  }

  std::vector<std::string> available;
  for (const auto *name : kActivationVariables)
    if (environment.contains(name))
      available.emplace_back(name);

  std::vector<std::string> systemdArguments{"--systemd"};
  systemdArguments.insert(systemdArguments.end(), available.begin(),
                          available.end());
  auto *process = &activationProcess_;
  const auto fallbackArguments = available;
  const auto started = process->run(
      tool, systemdArguments,
      [process, tool, fallbackArguments, environment,
       finish](bool success, const std::string &output) {
        if (success) {
          finish(true, {});
          return;
        }

        const auto firstDiagnostic = diagnostic(output);
        if (!process->run(
                tool, fallbackArguments,
                [finish, firstDiagnostic](bool fallbackSuccess,
                                          const std::string &fallbackOutput) {
                  finish(
                      fallbackSuccess,
                      fallbackSuccess
                          ? std::string{}
                          : (!diagnostic(fallbackOutput).empty()
                                 ? diagnostic(fallbackOutput)
                                 : (!firstDiagnostic.empty()
                                        ? firstDiagnostic
                                        : "Activation environment publication "
                                          "failed or timed out.")));
                },
                3000, environment)) {
          finish(false, "Could not start activation environment helper.");
        }
      },
      3000, environment);
  if (!started)
    finish(false, "Could not start activation environment helper.");
}

bool SessionServices::startSystemctl(const ProcessEnvironment &environment,
                                     const std::vector<std::string> &services,
                                     Completion completion) {
  const auto callback = std::make_shared<Completion>(std::move(completion));
  const auto finish = [callback] {
    if (!*callback)
      return;
    auto completion = std::move(*callback);
    *callback = {};
    completion();
  };
  const auto systemctl = findExecutable("systemctl");
  if (systemctl.empty()) {
    finish();
    return false;
  }

  std::vector<std::string> arguments{"--user", "restart"};
  arguments.insert(arguments.end(), services.begin(), services.end());
  const bool started = systemctlProcess_.run(
      systemctl, arguments,
      [finish](bool success, const std::string &output) {
        if (!success) {
          const auto message = diagnostic(output);
          if (!message.empty())
            std::cerr << "LunaDash portal service refresh: " << message << '\n';
        }
        finish();
      },
      5000, environment);
  if (!started)
    finish();
  return started;
}

void SessionServices::refreshScreencastPortalServices(
    const ProcessEnvironment &environment, Completion completion) {
  const std::filesystem::path config(LUDASH_XDPW_CONFIG_PATH);
  std::error_code filesystemError;
  const bool hasConfig =
      std::filesystem::is_regular_file(config, filesystemError) &&
      !filesystemError;
  const auto frontendServices = std::vector<std::string>{
      "xdg-desktop-portal-lunadash.service", "xdg-desktop-portal-gtk.service",
      "xdg-desktop-portal.service"};

  const auto xdpw = findXdpw();
  if (!xdpw.empty() && hasConfig) {
    if (portalProcess_.busy()) {
      startSystemctl(environment, frontendServices, std::move(completion));
      return;
    }

    const std::vector<std::string> arguments{
        "--replace", "--config", config.string(), "--loglevel", "INFO"};
    const bool started = portalProcess_.run(
        xdpw, arguments,
        [](bool success, const std::string &output) {
          if (!success) {
            const auto message = diagnostic(output);
            std::cerr << "LunaDash xdg-desktop-portal-wlr exited"
                      << (message.empty() ? "." : ": " + message) << '\n';
          }
        },
        0, environment);
    if (started) {
      startSystemctl(environment, frontendServices, std::move(completion));
      return;
    }
    std::cerr << "LunaDash could not start xdg-desktop-portal-wlr.\n";
    startSystemctl(environment, frontendServices, std::move(completion));
    return;
  }

  startSystemctl(
      environment,
      {"xdg-desktop-portal-wlr.service", "xdg-desktop-portal-lunadash.service",
       "xdg-desktop-portal-gtk.service", "xdg-desktop-portal.service"},
      std::move(completion));
}

} // namespace LunaDash
