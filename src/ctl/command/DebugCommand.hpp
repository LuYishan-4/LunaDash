#pragma once

#include "ctl/command/ControlClient.hpp"

namespace LunaDash {
// `lunadashctl debug`: readable diagnostics from the compositor's `debug` and `status`
// answers, a live watch, the session log and a bug-report bundle.
class DebugCommand final {
public:
  explicit DebugCommand(ControlClient &client) : client_(client) {}
  int run(int argc, char **argv);

private:
  ControlClient &client_;
};
} // namespace LunaDash
