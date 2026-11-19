#include "compositor/session/SessionAutomation.hpp"

#include "compositor/wayland/WaylandCompositor.hpp"

#include <algorithm>
#include <string>

namespace LunaDash {

bool scheduleSessionAutomation(WaylandCompositor &compositor,
                               const CompositorOptions &options) {
  if (options.demo &&
      !compositor.scheduleEventLoopCallback(
          3000, [&compositor, shell = !options.noShell] {
            if (shell)
              compositor.sendShellRequest("welcome");
          }))
    return false;

  if (!options.exitAfterSet)
    return true;
  return compositor.scheduleEventLoopCallback(
      std::max(1000, options.exitAfterMilliseconds),
      [&compositor, state = options.state] {
        if (!state.empty())
          compositor.saveState(state);
        compositor.closeTestSession([&compositor](bool clean) {
          compositor.terminateEventLoop(
              clean && !compositor.hasProcessFailure() ? 0 : 2);
        });
      });
}

} // namespace LunaDash
