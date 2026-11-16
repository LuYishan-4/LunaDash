#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/input/Keyboard.hpp"
#include "compositor/session/SessionActions.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "config/localization/Localization.hpp"
#include "core/log/Log.hpp"
#include "desktop/system/UpdateChecker.hpp"

namespace LunaDash {

void CompositorCommands::registerSessionCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  // Translations are only sent when the caller's language differs.
  registry.add("status", [](C &c, const R &r) -> CommandResult {
    return c.state(r.value != selectedLanguage());
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
  registry.add("check-update", [](C &c, const R &) -> CommandResult {
    c.updateChecker_->check();
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
