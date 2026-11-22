#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/Register.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

namespace LunaDash {
namespace {
bool belongsToProcess(std::int64_t candidate, std::int64_t launched) {
  if (launched <= 0)
    return false;
  for (int depth = 0; candidate > 1 && depth < 16; ++depth) {
    if (candidate == launched)
      return true;
    std::ifstream file("/proc/" + std::to_string(candidate) + "/stat");
    std::string stat;
    if (!std::getline(file, stat))
      return false;
    const auto end = stat.rfind(')');
    if (end == std::string::npos)
      return false;
    std::istringstream fields(stat.substr(end + 2));
    char state = '\0';
    std::int64_t parent = 0;
    if (!(fields >> state >> parent))
      return false;
    if (parent == candidate)
      return false;
    candidate = parent;
  }
  return false;
}
} // namespace

bool WaylandCompositor::toggleScratchpad(std::string *error) {
  for (const auto &client : clients_) {
    if (client->id != scratchpad_.window)
      continue;
    const bool hide = client->state.mapped && !client->state.minimized &&
                      client->state.workspace == workspace_ &&
                      focused_ == client.get();
    client->state.workspace = workspace_;
    client->state.minimized = hide;
    client->state.floating = true;
    client->state.maximized = false;
    arrange();
    if (hide)
      synchronizeWindowFocus();
    else
      focus(client.get());
    return true;
  }
  scratchpad_.window = 0;
  if (scratchpad_.pending)
    return true;
  std::string nativeError;
  auto command = defaultApplicationCommand("terminal", &nativeError);
  if (error)
    *error = (nativeError);
  if (command.empty())
    return false;
  scratchpad_.pending = true;
  const auto generation = ++scratchpad_.generation;
  scratchpad_.error.clear();
  const auto executable = (command.front());
  std::vector<std::string> arguments;
  for (auto argument = std::next(command.begin()); argument != command.end();
       ++argument)
    arguments.push_back((*argument));
  auto *process = spawn(arguments, executable, false,
                        [this, generation](const ProcessResult &result) {
                          if (result.success || !scratchpad_.pending ||
                              scratchpad_.generation != generation)
                            return;
                          d->cancelScratchpadTimeout();
                          scratchpad_.pending = false;
                          scratchpad_.process = 0;
                          scratchpad_.error =
                              "Could not launch the configured terminal.";
                        });
  if (!process) {
    scratchpad_.pending = false;
    if (error)
      *error = "Could not launch the configured terminal.";
    return false;
  }
  scratchpad_.process = process->processId();
  if (!d->scheduleScratchpadTimeout(generation)) {
    scratchpad_.pending = false;
    scratchpad_.error = "Could not monitor the configured terminal launch.";
  }
  return true;
}

void WaylandCompositor::adoptScratchpad(ClientWindow *client) {
  if (client && client->id == scratchpad_.window) {
    client->state.floating = true;
    return;
  }
  if (!client || !scratchpad_.pending || client->utility ||
      !belongsToProcess(client->processId, scratchpad_.process))
    return;
  scratchpad_.window = client->id;
  d->cancelScratchpadTimeout();
  scratchpad_.pending = false;
  scratchpad_.process = 0;
  client->state.floating = true;
  client->state.workspace = workspace_;
  client->layout.preferredFloatingSize = Size(960, 540);
}
} // namespace LunaDash
