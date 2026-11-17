#include "compositor/session/FatalSignalDiagnostics.hpp"

#include <csignal>
#include <initializer_list>
#include <unistd.h>

namespace LunaDash {
namespace {

void fatalSignalHandler(int signalNumber) {
  static constexpr char message[] =
      "LunaDash compositor received a fatal signal.\n";
  const auto written = ::write(STDERR_FILENO, message, sizeof(message) - 1);
  // Best-effort diagnostic: a failed or partial write must not delay the signal.
  (void)written;
  ::signal(signalNumber, SIG_DFL);
  ::kill(::getpid(), signalNumber);
}

} // namespace

void installFatalSignalDiagnostics() {
  for (const int signalNumber : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE}) {
    struct sigaction action {};
    action.sa_handler = fatalSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;
    sigaction(signalNumber, &action, nullptr);
  }
}

} // namespace LunaDash
