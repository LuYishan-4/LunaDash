#include "compositor/session/SessionHelpers.hpp"

#include "core/filesystem/FileIo.hpp"

namespace LunaDash {

void registerBuiltinSessionHelpers(SessionHelperRegistry &registry) {
  // The LunaDash shell is the polkit agent itself (its own authentication
  // dialog); another agent started first would take the session's agent
  // slot. Other shells, or none, get the distribution's agent.
  registry.add("polkit-agent",
               {[](const SessionConfig &session) -> std::vector<std::string> {
                 if (session.shell == SessionConfig::Shell::LunaDash)
                   return {};
                 return {"lunadash-polkit-agent"};
               }});
  // session.json "inputMethodCommand". LUNADASH_DISABLE_FCITX=1 (its historic
  // name) skips the input method for one session, whichever one is configured.
  registry.add("input-method",
               {[](const SessionConfig &session) -> std::vector<std::string> {
                 if (environmentInt("LUNADASH_DISABLE_FCITX") == 1)
                   return {};
                 return session.inputMethodCommand;
               }});
}

} // namespace LunaDash
