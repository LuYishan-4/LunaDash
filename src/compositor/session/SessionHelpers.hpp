#pragma once

#include "config/session/SessionConfig.hpp"
#include "core/templates/NamedRegistry.hpp"

#include <functional>
#include <string>
#include <vector>

namespace LunaDash {

// A background program a desktop session starts before its shell (a polkit
// agent, an input method). Started only when found on PATH.
struct SessionHelper {
  std::string program;
  std::vector<std::string> arguments;
  // Whether this session wants the helper.
  std::function<bool(const SessionConfig &)> wanted;
};

using SessionHelperRegistry = Templates::NamedRegistry<SessionHelper>;

void registerBuiltinSessionHelpers(SessionHelperRegistry &registry);

} // namespace LunaDash
