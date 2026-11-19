#pragma once

#include "core/json/JsonAccess.hpp"
#include "core/templates/NamedRegistry.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace LunaDash {

class WaylandCompositor;


struct CommandRequest {
  std::string_view method;
  const std::string &value;
  const Json &request;
};


using CommandResult = std::optional<Json>;
using CommandHandler =
    std::function<CommandResult(WaylandCompositor &, const CommandRequest &)>;

using CommandRegistry = Templates::NamedRegistry<CommandHandler>;

inline CommandResult commandError(std::string message) {
  return Json{{"error", std::move(message)}};
}


inline CommandResult invalidCommand() {
  return commandError("Invalid command or value");
}

} // namespace LunaDash
