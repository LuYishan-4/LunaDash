#pragma once
#include <map>
#include <string>
namespace LunaDash {
using ShellRenderingEnvironment = std::map<std::string, std::string>;
// Quickshell (legacy QML shell): translates LUDASH_SHELL_RENDERER into Qt settings.
bool configureShellRendering(ShellRenderingEnvironment &environment,
                             bool nvidiaDriver);
// SlintLu (Slint shell): translates LUDASH_SHELL_RENDERER into SLINTLU_RENDERER. "auto"
// keeps an explicit SLINTLU_RENDERER, otherwise SlintLu chooses and falls back itself.
bool configureSlintLuRendering(ShellRenderingEnvironment &environment);
} // namespace LunaDash
