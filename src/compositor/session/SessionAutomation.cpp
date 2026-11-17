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
            compositor.spawn({"--app", "files"});
          }))
    return false;

  if (!options.exitAfterSet)
    return true;
  return compositor.scheduleEventLoopCallback(
      std::max(1000, options.exitAfterMilliseconds),
      [&compositor, state = options.state, screenshot = options.screenshot] {
        if (!state.empty())
          compositor.saveState(state);
        const auto finish = [&compositor](bool ok) {
          compositor.closeTestSession([&compositor, ok](bool clean) {
            compositor.terminateEventLoop(
                ok && clean && !compositor.hasProcessFailure() ? 0 : 2);
          });
        };
        if (screenshot.empty()) {
          finish(true);
          return;
        }
        if (!compositor.saveScreenshot(screenshot, finish))
          finish(false);
      });
}

} // namespace LunaDash
