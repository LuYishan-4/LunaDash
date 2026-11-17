#pragma once

#include "config/session/SessionConfig.hpp"
#include "core/templates/NamedRegistry.hpp"

#include <functional>
#include <string>
#include <vector>

namespace LunaDash {

// A background program a desktop session starts before its shell (a polkit
// agent, an input method). Started only when its program is found on PATH.
struct SessionHelper {
  // The program and arguments this session starts, or an empty command when
  // the session wants no such helper.
  std::function<std::vector<std::string>(const SessionConfig &)> command;
};

using SessionHelperRegistry = Templates::NamedRegistry<SessionHelper>;

void registerBuiltinSessionHelpers(SessionHelperRegistry &registry);

} // namespace LunaDash
