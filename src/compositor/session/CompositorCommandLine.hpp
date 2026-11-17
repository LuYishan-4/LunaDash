#pragma once

#include "compositor/session/CompositorOptions.hpp"

#include <optional>

namespace LunaDash {

// Parses the compositor command line. Prints a diagnostic to stderr and
// returns nothing when an option is unknown or has an invalid value.
std::optional<CompositorOptions> parseCompositorOptions(int argc, char **argv);

// Runs an option that finishes without starting the compositor (--help,
// --version, --set-language, --session-shell) and returns its exit code.
// Returns nothing when the compositor should start.
std::optional<int> runOneShotOption(const CompositorOptions &options);

} // namespace LunaDash
