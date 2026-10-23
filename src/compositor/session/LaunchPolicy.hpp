#pragma once

#include <string>
#include <vector>

namespace LunaDash {

struct LaunchIdentity {
  std::string desktopId;
  std::string flatpakId;
  std::string executable;
};

struct LaunchCapabilities {
  bool x11Helper = false;
};

LaunchIdentity identifyLaunch(const std::string &desktopId,
                              const std::vector<std::string> &command);
LaunchCapabilities resolveLaunchCapabilities(
    const std::string &desktopId, const std::vector<std::string> &command);

} // namespace LunaDash
