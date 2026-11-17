#include "compositor/session/CompositorCommandLine.hpp"
#include "compositor/session/FatalSignalDiagnostics.hpp"
#include "compositor/session/SessionAutomation.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"

int main(int argc, char **argv) {
  LunaDash::installFatalSignalDiagnostics();
  const auto options = LunaDash::parseCompositorOptions(argc, argv);
  if (!options)
    return 1;
  if (const auto exitCode = LunaDash::ChooseOption(*options))
    return *exitCode;
  LunaDash::WaylandCompositor compositor(*options);
  if (!compositor.start() ||
      !LunaDash::scheduleSessionAutomation(compositor, *options))
    return 2;
  return compositor.runEventLoop(options->profile);
}
