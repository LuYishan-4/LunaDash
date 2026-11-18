#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/input/Keyboard.hpp"
#include "compositor/session/SessionActions.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "core/log/Log.hpp"

namespace LunaDash {

void CompositorCommands::registerSessionCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  registry.add("status", [](C &c, const R &) -> CommandResult {
    return c.state();
  });
  registry.add("debug", [](C &c, const R &) -> CommandResult {
    return c.debugSnapshot();
  });
  registry.add("session-action", [](C &c, const R &r) -> CommandResult {
    std::string error;
    if (!isValidSessionAction(r.value) || !c.sessionActions_->execute(r.value, &error))
      return commandError(error.empty() ? "Invalid session action." : error);
    return {};
  });
  registry.add("quit", [](C &c, const R &r) -> CommandResult {
    if (r.value != "confirm")
      return commandError("Logout requires explicit confirmation.");
    logInfo("LunaDash logout requested through the control channel.");
    c.d->scheduleCallback(0, [&c] { c.requestShutdown(); });
    return {};
  });
  // Editing shortcuts (copy, paste, cut, selectAll) for the focused application.
  registry.add("send-key", [](C &c, const R &r) -> CommandResult {
    std::string error;
    if (!sendEditingKeyChord(c.d ? c.d->seat : nullptr, r.value, &error))
      return commandError(error);
    return {};
  });
}

} // namespace LunaDash
