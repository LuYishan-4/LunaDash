#pragma once

namespace LunaDash {

// Writes a one-line diagnostic to stderr on SIGSEGV, SIGABRT, SIGBUS, SIGILL
// and SIGFPE, then re-raises the signal with its default action.
void installFatalSignalDiagnostics();

} // namespace LunaDash
