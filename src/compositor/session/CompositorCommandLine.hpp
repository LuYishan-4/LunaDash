#pragma once

#include "compositor/session/CompositorOptions.hpp"

#include <optional>

namespace LunaDash {


std::optional<CompositorOptions> parseCompositorOptions(int argc, char **argv);


std::optional<int> ChooseOption(const CompositorOptions &options);

} // namespace LunaDash
