#pragma once

namespace LunaDash::SessionTuning {

// Welcome or the startup apps wait this long so the shell's layers map first.
inline constexpr int startupAppsDelayMs = 1200;
// Shell restarts back off exponentially: base << min(failures, maxExponent), capped.
inline constexpr int shellRestartBaseMs = 250;
inline constexpr int shellRestartMaxMs = 4000;
inline constexpr int shellRestartMaxExponent = 4;
// A shell still running after this long resets the restart back-off.
inline constexpr int shellHealthyAfterMs = 10000;
// closeTestSession() polls for clients, helpers and XWayland to finish.
inline constexpr int testSessionPollMs = 50;
inline constexpr int testSessionTimeoutMs = 5000;

} // namespace LunaDash::SessionTuning
