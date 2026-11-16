#include "compositor/session/SessionHelpers.hpp"

#include "core/filesystem/FileIo.hpp"

namespace LunaDash {

void registerBuiltinSessionHelpers(SessionHelperRegistry &registry) {
  // The LunaDash shell is the polkit agent itself (its own authentication
  // dialog); another agent started first would take the session's agent
  // slot. Other shells, or none, get the distribution's agent.
  registry.add("polkit-agent",
               {"lunadash-polkit-agent", {}, [](const SessionConfig &session) {
                  return session.shell != SessionConfig::Shell::LunaDash;
                }});
  registry.add("input-method", {"fcitx5", {"--replace"}, [](const SessionConfig &) {
                 return environmentInt("LUNADASH_DISABLE_FCITX") != 1;
               }});
}

} // namespace LunaDash
