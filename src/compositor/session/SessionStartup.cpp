#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/session/SessionServices.hpp"
#include "compositor/session/SessionTuning.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/xwayland/XWaylandSupport.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/log/Log.hpp"
#include "core/process/ExecutableLookup.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace LunaDash {

void WaylandCompositor::startSessionClients(bool startShell) {
  if (!startShell || lifecycle_.shuttingDown || lifecycle_.testStopping)
    return;

  session_ = loadSessionConfig();
  if (!session_.error.empty())
    logWarning("Ignoring %s: %s", session_.source.c_str(), session_.error.c_str());

  // Session helpers belong to a real desktop session (one that publishes its
  // activation environment), not to nested or test compositors.
  if (environmentInt("LUNADASH_PUBLISH_ACTIVATION_ENV") == 1)
    for (const auto &name : sessionHelpers_.names()) {
      const auto *helper = sessionHelpers_.find(name);
      const auto command =
          helper->command ? helper->command(session_) : std::vector<std::string>{};
      if (command.empty())
        continue;
      const std::string program = findExecutable(command.front());
      if (!program.empty())
        spawn({command.begin() + 1, command.end()}, program, false);
    }
  std::string error;
  switch (session_.shell) {
  case SessionConfig::Shell::LunaDash:
    spawn({"--session"}, {}, false);
    break;
  case SessionConfig::Shell::Custom:
    logInfo("LunaDash shell: custom (%s)", session_.shellCommand.front().c_str());
    if (!launchExternalCommand(session_.shellCommand, &error))
      logWarning("The custom shell could not start: %s", error.c_str());
    break;
  case SessionConfig::Shell::None:
    logInfo("LunaDash shell: none");
    break;
  }
  for (const auto &command : session_.autostart)
    if (!launchExternalCommand(command, &error))
      logWarning("Autostart %s failed: %s", command.front().c_str(), error.c_str());

  // A first session shows the shell's Welcome, which the shell opens by itself and
  // which finishes setup (finish-setup) when closed; later sessions start the user's
  // startup apps, after a short wait so the shell's layers map first.
  d->scheduleCallback(SessionTuning::startupAppsDelayMs, [this] {
    if (lifecycle_.testStopping || lifecycle_.shuttingDown)
      return;
    // Setup belongs to the LunaDash desktop; other shells bring their own first run.
    if (!setupComplete() && session_.shell == SessionConfig::Shell::LunaDash)
      return;
    for (const auto &app :
         desktopPreferences().value("startupApps", nlohmann::json::array()))
      // Retired apps: Welcome is part of the shell, the package manager is gone.
      if (app.is_string() && app != "welcome" && app != "packages")
        spawn({"--app", app.get<std::string>()});
  });
}

void WaylandCompositor::scheduleShellRestart() {
  if (lifecycle_.shuttingDown || lifecycle_.testStopping ||
      lifecycle_.logoutPending || lifecycle_.shellRestartScheduled)
    return;

  const int exponent = std::min(lifecycle_.shellRestartFailures,
                                SessionTuning::shellRestartMaxExponent);
  const int delayMs = std::min(SessionTuning::shellRestartMaxMs,
                               SessionTuning::shellRestartBaseMs * (1 << exponent));
  ++lifecycle_.shellRestartFailures;
  lifecycle_.shellRestartScheduled = true;
  logWarning("LunaDash shell restart scheduled in %d ms", delayMs);

  d->scheduleCallback(delayMs, [this] {
    lifecycle_.shellRestartScheduled = false;
    if (lifecycle_.shuttingDown || lifecycle_.testStopping ||
        lifecycle_.logoutPending)
      return;
    spawn({"--session"}, {}, false);
  });
}

bool WaylandCompositor::launchExternalCommand(std::vector<std::string> command,
                                              std::string *error, bool x11Helper) {
  if (command.empty() || command.front().empty()) {
    *error = "Expected an application command.";
    return false;
  }

  // Application/toolkit renderer selection belongs to the client. Do not
  // identify Chromium/Electron applications by executable names or rewrite
  // their ANGLE, Vulkan, GPU, Ozone or IME flags here. Besides being brittle,
  // scanning every command argument can misclassify URLs and document paths.
  //
  // XWayland is likewise opt-in for the launch request. Native Wayland clients
  // may still use DISPLAY from the prepared session environment when they
  // intentionally create an X11 helper, without LunaDash special-casing an
  // application such as Discord.
  if (x11Helper) {
    std::string xwaylandError;
    if (!xwayland_ || !xwayland_->startServer(&xwaylandError)) {
      if (xwaylandError.empty())
        xwaylandError =
            "This application's helper requires XWayland. Install and enable "
            "it, then restart LunaDash.";
      if (error)
        *error = xwaylandError;
      return false;
    }
  }

  const std::string executable = command.front();
  command.erase(command.begin());
  spawn(command, executable, false);
  return true;
}

void WaylandCompositor::publishSessionActivationEnvironment(
    const std::function<void()> &ready) {
  if (environmentInt("LUNADASH_PUBLISH_ACTIVATION_ENV") != 1) {
    if (ready)
      ready();
    return;
  }
  auto environment = clientEnvironment_;
  applyXWaylandEnvironment(environment, xwayland_.get());
  if (!sessionServices_) {
    if (ready)
      ready();
    return;
  }
  sessionServices_->publishActivationEnvironment(
      environment,
      [this, ready](bool ok, const std::string &error) {
        activationEnvironment_.published = ok;
        activationEnvironment_.error = error;
        if (!ok) {
          if (ready)
            ready();
          return;
        }
        sessionServices_->refreshScreencastPortalServices(
            clientEnvironment_, ready);
      });
}

void WaylandCompositor::closeTestSession(
    const std::function<void(bool)> &finished) {
  lifecycle_.testStopping = true;
  for (const auto &client : clients_)
    closeClient(client.get());

  auto elapsed = std::make_shared<int>(0);
  auto poll = std::make_shared<std::function<void()>>();
  const std::weak_ptr<std::function<void()>> weakPoll = poll;
  *poll = [this, elapsed, finished, weakPoll] {
    *elapsed += SessionTuning::testSessionPollMs;
    if (clients_.empty() && xwayland_)
      xwayland_->stop();
    const bool xwaylandDone = !xwayland_ || xwayland_->stopped();
    const bool processesDone = std::all_of(
        processes_.cbegin(), processes_.cend(), [](const auto &process) {
          return !process || !process->busy();
        });
    if ((clients_.empty() && processesDone && xwaylandDone) ||
        *elapsed >= SessionTuning::testSessionTimeoutMs) {
      finished(clients_.empty() && processesDone && xwaylandDone &&
               !lifecycle_.processFailure);
      return;
    }
    if (const auto next = weakPoll.lock()) {
      if (!d->scheduleCallback(SessionTuning::testSessionPollMs,
                               [next] { (*next)(); }))
        finished(false);
    }
  };
  if (!d->scheduleCallback(SessionTuning::testSessionPollMs, [poll] { (*poll)(); }))
    finished(false);
}

void WaylandCompositor::requestShutdown() {
  lifecycle_.logoutPending = true;
  bool applications = false;
  for (const auto &client : clients_) {
    if (client->utility)
      continue;
    applications = true;
    closeClient(client.get());
  }
  if (!applications)
    terminateEventLoop(hasProcessFailure() ? 2 : 0);
}

} // namespace LunaDash
