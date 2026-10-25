#pragma once

#include <optional>
#include <string>

namespace LunaDash {
class ControlClient final {
public:
  int run(int argc, char **argv);
  // One request on the compositor's control socket; the raw response line, or nullopt
  // when the socket cannot be reached or does not answer in time.
  std::optional<std::string> request(const std::string &method, const std::string &value,
                                     int timeoutMilliseconds = 1500);
  // $LUNADASH_CONTROL, or the session's control socket in the runtime directory.
  static std::string socketPath();
};
} // namespace LunaDash
