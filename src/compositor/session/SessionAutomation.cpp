#include "compositor/session/SessionAutomation.hpp"

#include "compositor/wayland/WaylandCompositor.hpp"

#include <algorithm>
#include <string>

namespace LunaDash {

bool scheduleSessionAutomation(WaylandCompositor &compositor,
                               const CompositorOptions &options) {
  if (options.demo &&
      !compositor.scheduleEventLoopCallback(
          900, [&compositor, shell = !options.noShell] {
            if (shell)
              compositor.spawn({"--app", "welcome"});
            //compositor.spawn({"--app", "files"});
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
