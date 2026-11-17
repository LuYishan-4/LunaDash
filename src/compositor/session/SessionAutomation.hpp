#pragma once

#include "compositor/session/CompositorOptions.hpp"

namespace LunaDash {

class WaylandCompositor;

// Schedules the automated parts of a session that the command line requests:
// --demo clients, and --exit-after with its --state and --screenshot output.
// Returns false when the event loop cannot schedule them.
bool scheduleSessionAutomation(WaylandCompositor &compositor,
                               const CompositorOptions &options);

} // namespace LunaDash
