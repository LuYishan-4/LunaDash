#pragma once

namespace LunaDash::SessionTuning {


inline constexpr int startupAppsDelayMs = 1200;

inline constexpr int shellRestartBaseMs = 250;
inline constexpr int shellRestartMaxMs = 4000;
inline constexpr int shellRestartMaxExponent = 4;

inline constexpr int shellHealthyAfterMs = 10000;

inline constexpr int testSessionPollMs = 50;
inline constexpr int testSessionTimeoutMs = 5000;

} // namespace LunaDash::SessionTuning
