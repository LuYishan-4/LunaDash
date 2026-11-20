#include "compositor/session/CompositorCommandLine.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"

#include <algorithm>
#include <csignal>
#include <initializer_list>
#include <unistd.h>

int main(int argc, char **argv) {
  const auto fatalSignalHandler = [](int signalNumber) {
    static constexpr char message[] =
        "LunaDash compositor received a fatal signal.\n";
    const auto written = ::write(STDERR_FILENO, message, sizeof(message) - 1);
    (void)written;
    ::signal(signalNumber, SIG_DFL);
    ::kill(::getpid(), signalNumber);
  };
  for (const int signalNumber : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE}) {
    struct sigaction action {};
    action.sa_handler = fatalSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;
    sigaction(signalNumber, &action, nullptr);
  }

  const auto options = LunaDash::parseCompositorOptions(argc, argv);
  if (!options)
    return 1;
  if (const auto exitCode = LunaDash::chooseOption(*options))
    return *exitCode;
  LunaDash::WaylandCompositor compositor(*options);
  if (!compositor.start())
    return 2;
  if (options->demo &&
      !compositor.scheduleEventLoopCallback(
          3000, [&compositor, shell = !options->noShell] {
            if (shell)
              compositor.sendShellRequest("welcome");
          }))
    return 2;
  if (options->exitAfterSet &&
      !compositor.scheduleEventLoopCallback(
          std::max(1000, options->exitAfterMilliseconds),
          [&compositor, state = options->state] {
            if (!state.empty())
              compositor.saveState(state);
            compositor.closeTestSession([&compositor](bool clean) {
              compositor.terminateEventLoop(
                  clean && !compositor.hasProcessFailure() ? 0 : 2);
            });
          }))
    return 2;
  return compositor.runEventLoop(options->profile);
}
