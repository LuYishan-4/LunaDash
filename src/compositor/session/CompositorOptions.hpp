#pragma once

#include <string>

namespace LunaDash {

struct CompositorOptions {
  std::string graphics = "auto";
  std::string socket = "lunadash-0";
  std::string state;
  int exitAfterMilliseconds = 0;
  bool exitAfterSet = false;
  bool profile = false;
  bool fullscreen = false;
  bool noShell = false;
  bool demo = false;
  bool help = false;
  bool version = false;
  bool sessionShell = false;
};

} // namespace LunaDash
