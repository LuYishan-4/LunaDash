#include "compositor/session/SessionServices.hpp"
#include "core/process/ExecutableLookup.hpp"

#include <algorithm>
#include <cstdlib>
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

std::string findSessionExecutable(std::string_view name) {
  const char *pathValue = std::getenv("PATH");
  return pathValue && *pathValue
             ? findExecutable(name, pathValue)
             : findExecutable(name, "/usr/local/bin:/usr/bin:/bin");
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
    : activationProcess_(eventLoop), systemctlProcess_(eventLoop) {}

void SessionServices::publishActivationEnvironment(
    const ProcessEnvironment &environment,
    std::function<void(bool, const std::string &)> completion) {
  const auto callback =
      std::make_shared<std::function<void(bool, const std::string &)>>(
          std::move(completion));
  const auto finish = [callback](bool success, const std::string &message) {
    if (!*callback)
      return;
    auto completion = std::move(*callback);
    *callback = {};
    completion(success, message);
  };
  const auto tool = findSessionExecutable("dbus-update-activation-environment");
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
                                     std::function<void()> completion) {
  const auto callback =
      std::make_shared<std::function<void()>>(std::move(completion));
  const auto finish = [callback] {
    if (!*callback)
      return;
    auto completion = std::move(*callback);
    *callback = {};
    completion();
  };
  const auto systemctl = findSessionExecutable("systemctl");
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
    const ProcessEnvironment &environment, std::function<void()> completion) {
  startSystemctl(
      environment,
      {"xdg-desktop-portal-wlr.service", "xdg-desktop-portal-gnome.service",
       "xdg-desktop-portal-lunadash.service", "xdg-desktop-portal-gtk.service",
       "xdg-desktop-portal.service"},
      std::move(completion));
}

} // namespace LunaDash
