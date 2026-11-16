#pragma once

#include "core/json/JsonAccess.hpp"
#include "core/templates/NamedRegistry.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace LunaDash {

class WaylandCompositor;

// One control-channel request: {"method": ..., "value": ...}.
struct CommandRequest {
  std::string_view method;
  const std::string &value;
  const Json &request;
};

// A reply, or nothing to reply with the compositor state.
using CommandResult = std::optional<Json>;
using CommandHandler =
    std::function<CommandResult(WaylandCompositor &, const CommandRequest &)>;
// Control-channel methods. The compositor dispatches requests through it and
// registers its built-in methods per domain (CompositorCommands).
using CommandRegistry = Templates::NamedRegistry<CommandHandler>;

inline CommandResult commandError(std::string message) {
  return Json{{"error", std::move(message)}};
}

// The reply for an unknown method or a value the method does not accept.
inline CommandResult invalidCommand() {
  return commandError("Invalid command or value");
}

} // namespace LunaDash
