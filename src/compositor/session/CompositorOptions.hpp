#pragma once

#include <string>

namespace LunaDash {

// Command-line options of lunadash-compositor, parsed by
// parseCompositorOptions() (CompositorCommandLine.cpp). The member defaults are
// the defaults of a session started without options.
struct CompositorOptions {
  // Renderer preference: auto, opengl, gles or vulkan (validated, lowercase).
  std::string graphics = "auto";
  std::string socket = "lunadash-0";
  // Test runs: written when --exit-after ends the session.
  std::string screenshot;
  std::string state;
  // --set-language: store the desktop language and exit (the installer uses it).
  std::string setLanguage;
  int exitAfterMilliseconds = 0;
  bool exitAfterSet = false;
  bool profile = false;
  // Nested only: size the output like the host window instead of the
  // configured virtual output size.
  bool fullscreen = false;
  bool noShell = false;
  bool demo = false;
  bool help = false;
  bool version = false;
  bool sessionShell = false;
};

} // namespace LunaDash
